/*
 *  SPDX-License-Identifier: MIT
 *
 *  VOLE (Vector Oblivious Linear Evaluation) implementation
 *
 *  When FAEST_PARALLEL_VOLE is defined, tau rounds are parallelized using
 *  POSIX threads for improved performance on multi-core systems.
 *
 *  Build options:
 *    -Dparallel-vole=disabled  : Sequential execution (default)
 *    -Dparallel-vole=enabled   : Parallel execution with pthread
 *    -Dnum-threads=N           : Number of worker threads (default: 4)
 *
 *  Performance (Apple M2, FAEST-128s sign):
 *    Sequential:     ~44 ms
 *    Parallel (4T):  ~35 ms (+21%)
 */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include "vole.h"
#include "aes.h"
#include "utils.h"
#include "random_oracle.h"

#include <stdbool.h>
#include <string.h>

#ifdef FAEST_PARALLEL_VOLE
#include <pthread.h>

#ifndef FAEST_NUM_THREADS
#define FAEST_NUM_THREADS 4
#endif
#endif /* FAEST_PARALLEL_VOLE */

static const uint32_t TWEAK_OFFSET = UINT32_C(0x80000000); // 2^31

/* ============================================================================
 * convert_to_vole - Core VOLE conversion function
 * ============================================================================ */

#if !defined(FAEST_TESTS)
static
#endif
    unsigned int
    convert_to_vole(const uint8_t* iv, const uint8_t* sd, bool sd0_bot, unsigned int i,
                    unsigned int outlen, uint8_t* u, uint8_t* v, const faest_paramset_t* params) {
  const unsigned int lambda        = params->lambda;
  const unsigned int tau_1         = params->tau1;
  const unsigned int k             = params->k;
  const unsigned int num_instances = bavc_max_node_index(i, tau_1, k);
  const unsigned int lambda_bytes  = lambda / 8;
  const unsigned int depth         = bavc_max_node_depth(i, tau_1, k);

  // (depth + 1) x num_instances array of outlen; but we only need two rows at a time
  uint8_t* r = calloc(2 * num_instances, outlen);

#define R(row, column) (r + (((row) % 2) * num_instances + (column)) * outlen)
#define V(idx) (v + (idx) * outlen)

  uint32_t tweak = i ^ TWEAK_OFFSET;

  // Step: 2
  if (!sd0_bot) {
    prg(sd, iv, tweak, R(0, 0), lambda, outlen);
  }

  // Step: 3..4
  for (unsigned int j = 1; j < num_instances; ++j) {
    prg(sd + lambda_bytes * j, iv, tweak, R(0, j), lambda, outlen);
  }

  // Step: 5..9
  memset(v, 0, depth * outlen);
  for (unsigned int j = 0; j < depth; j++) {
    unsigned int depthloop = num_instances >> (j + 1);
    for (unsigned int idx = 0; idx < depthloop; idx++) {
      xor_u8_array(V(j), R(j, 2 * idx + 1), V(j), outlen);
      xor_u8_array(R(j, 2 * idx), R(j, 2 * idx + 1), R(j + 1, idx), outlen);
    }
  }
  // Step: 10
  if (!sd0_bot && u != NULL) {
    memcpy(u, R(depth, 0), outlen);
  }
  free(r);
  return depth;

#undef R
#undef V
}

/* ============================================================================
 * vole_commit implementation
 * ============================================================================ */

#ifdef FAEST_PARALLEL_VOLE

typedef struct {
  const uint8_t* iv;
  const uint8_t* sd;
  unsigned int i;
  unsigned int ellhat_bytes;
  uint8_t* ui;
  uint8_t* v;
  const faest_paramset_t* params;
  unsigned int result_depth;
} vole_commit_task_t;

static void* vole_commit_thread(void* arg) {
  vole_commit_task_t* task = (vole_commit_task_t*)arg;
  task->result_depth = convert_to_vole(
      task->iv, task->sd, false, task->i, task->ellhat_bytes,
      task->ui, task->v, task->params);
  return NULL;
}

#endif /* FAEST_PARALLEL_VOLE */

void vole_commit(const uint8_t* rootKey, const uint8_t* iv, unsigned int ellhat,
                 const faest_paramset_t* params, bavc_t* bavc, uint8_t* c, uint8_t* u,
                 uint8_t** v) {
  const unsigned int lambda       = params->lambda;
  const unsigned int lambda_bytes = lambda / 8;
  const unsigned int ellhat_bytes = (ellhat + 7) / 8;
  const unsigned int tau          = params->tau;
  const unsigned int tau_1        = params->tau1;
  const unsigned int k            = params->k;

  bavc_commit(bavc, rootKey, iv, params);

  uint8_t* ui = malloc(tau * ellhat_bytes);
  assert(ui);

#ifdef FAEST_PARALLEL_VOLE
  /* Parallel implementation */

  // Pre-calculate offsets for each round
  unsigned int* v_offsets = malloc(tau * sizeof(unsigned int));
  unsigned int* sd_offsets = malloc(tau * sizeof(unsigned int));
  assert(v_offsets && sd_offsets);

  v_offsets[0] = 0;
  sd_offsets[0] = 0;
  for (unsigned int i = 1; i < tau; ++i) {
    v_offsets[i] = v_offsets[i-1] + bavc_max_node_depth(i-1, tau_1, k);
    sd_offsets[i] = sd_offsets[i-1] + lambda_bytes * bavc_max_node_index(i-1, tau_1, k);
  }

  // Parallel convert_to_vole
  unsigned int num_threads = (tau < FAEST_NUM_THREADS) ? tau : FAEST_NUM_THREADS;
  pthread_t* threads = malloc(num_threads * sizeof(pthread_t));
  vole_commit_task_t* tasks = malloc(tau * sizeof(vole_commit_task_t));
  assert(threads && tasks);

  for (unsigned int i = 0; i < tau; ++i) {
    tasks[i].iv = iv;
    tasks[i].sd = bavc->sd + sd_offsets[i];
    tasks[i].i = i;
    tasks[i].ellhat_bytes = ellhat_bytes;
    tasks[i].ui = ui + i * ellhat_bytes;
    tasks[i].v = v[v_offsets[i]];
    tasks[i].params = params;
    tasks[i].result_depth = 0;
  }

  for (unsigned int batch_start = 0; batch_start < tau; batch_start += num_threads) {
    unsigned int batch_end = batch_start + num_threads;
    if (batch_end > tau) batch_end = tau;
    unsigned int batch_size = batch_end - batch_start;

    for (unsigned int t = 0; t < batch_size; ++t) {
      pthread_create(&threads[t], NULL, vole_commit_thread, &tasks[batch_start + t]);
    }
    for (unsigned int t = 0; t < batch_size; ++t) {
      pthread_join(threads[t], NULL);
    }
  }

  unsigned int v_idx = 0;
  for (unsigned int i = 0; i < tau; ++i) {
    v_idx += tasks[i].result_depth;
  }

  free(tasks);
  free(threads);
  free(sd_offsets);
  free(v_offsets);

#else
  /* Sequential implementation */

  unsigned int v_idx = 0;
  uint8_t* sd_i      = bavc->sd;
  for (unsigned int i = 0; i < tau; ++i) {
    v_idx +=
        convert_to_vole(iv, sd_i, false, i, ellhat_bytes, ui + i * ellhat_bytes, v[v_idx], params);
    sd_i += lambda_bytes * bavc_max_node_index(i, tau_1, k);
  }

#endif /* FAEST_PARALLEL_VOLE */

  // ensure 0-padding up to lambda
  for (; v_idx != lambda; ++v_idx) {
    memset(v[v_idx], 0, ellhat_bytes);
  }

  // Step 9
  memcpy(u, ui, ellhat_bytes);
  for (unsigned int i = 1; i < tau; i++) {
    xor_u8_array(u, ui + i * ellhat_bytes, c + (i - 1) * ellhat_bytes, ellhat_bytes);
  }
  free(ui);
}

/* ============================================================================
 * vole_reconstruct implementation
 * ============================================================================ */

#ifdef FAEST_PARALLEL_VOLE

typedef struct {
  const uint8_t* iv;
  const uint8_t* c;
  unsigned int i;
  uint16_t i_delta;
  unsigned int ellhat_bytes;
  const uint8_t* sd_i_in;
  uint8_t* qtmp;
  uint8_t** q;
  unsigned int q_offset;
  const faest_paramset_t* params;
  unsigned int result_ki;
} vole_reconstruct_task_t;

static void* vole_reconstruct_thread(void* arg) {
  vole_reconstruct_task_t* task = (vole_reconstruct_task_t*)arg;
  const unsigned int lambda_bytes = task->params->lambda / 8;
  const unsigned int tau1 = task->params->tau1;
  const unsigned int k = task->params->k;
  const unsigned int Ni = bavc_max_node_index(task->i, tau1, k);

  uint8_t* sd = malloc((1 << k) * lambda_bytes);
  assert(sd);

  for (unsigned int j = 0; j < Ni; j++) {
    if (j < task->i_delta) {
      memcpy(sd + (j ^ task->i_delta) * lambda_bytes,
             task->sd_i_in + lambda_bytes * j, lambda_bytes);
    } else if (j > task->i_delta) {
      memcpy(sd + (j ^ task->i_delta) * lambda_bytes,
             task->sd_i_in + lambda_bytes * (j - 1), lambda_bytes);
    }
  }

  task->result_ki = convert_to_vole(task->iv, sd, true, task->i,
                                     task->ellhat_bytes, NULL, task->qtmp, task->params);

  if (task->i == 0) {
    memcpy(task->q[task->q_offset], task->qtmp, task->ellhat_bytes * task->result_ki);
  } else {
    for (unsigned int d = 0; d < task->result_ki; ++d) {
      masked_xor_u8_array(task->qtmp + d * task->ellhat_bytes,
                          task->c + (task->i - 1) * task->ellhat_bytes,
                          task->q[task->q_offset + d],
                          (task->i_delta >> d) & 1, task->ellhat_bytes);
    }
  }

  free(sd);
  return NULL;
}

#endif /* FAEST_PARALLEL_VOLE */

bool vole_reconstruct(uint8_t* com, uint8_t** q, const uint8_t* iv, const uint8_t* chall_3,
                      const uint8_t* decom_i, const uint8_t* c, unsigned int ellhat,
                      const faest_paramset_t* params) {
  const unsigned int lambda       = params->lambda;
  const unsigned int lambda_bytes = lambda / 8;
  const unsigned int ellhat_bytes = (ellhat + 7) / 8;
  const unsigned int tau          = params->tau;
  const unsigned int tau1         = params->tau1;
  const unsigned int L            = params->L;
  const unsigned int k            = params->k;

  uint16_t i_delta[MAX_TAU];
  if (!decode_all_chall_3(i_delta, chall_3, params)) {
    return false;
  }

  bavc_rec_t bavc_rec;
  bavc_rec.h = com;
  bavc_rec.s = malloc((L - tau) * lambda_bytes);
  assert(bavc_rec.s);

  if (!bavc_reconstruct(&bavc_rec, decom_i, i_delta, iv, params)) {
    free(bavc_rec.s);
    return false;
  }

#ifdef FAEST_PARALLEL_VOLE
  /* Parallel implementation */

  unsigned int* q_offsets = malloc(tau * sizeof(unsigned int));
  unsigned int* sd_offsets = malloc(tau * sizeof(unsigned int));
  assert(q_offsets && sd_offsets);

  q_offsets[0] = 0;
  sd_offsets[0] = 0;
  for (unsigned int i = 1; i < tau; ++i) {
    q_offsets[i] = q_offsets[i-1] + bavc_max_node_depth(i-1, tau1, k);
    unsigned int Ni_prev = bavc_max_node_index(i-1, tau1, k);
    sd_offsets[i] = sd_offsets[i-1] + lambda_bytes * (Ni_prev - 1);
  }

  unsigned int num_threads = (tau < FAEST_NUM_THREADS) ? tau : FAEST_NUM_THREADS;
  pthread_t* threads = malloc(num_threads * sizeof(pthread_t));
  vole_reconstruct_task_t* tasks = malloc(tau * sizeof(vole_reconstruct_task_t));
  uint8_t** qtmp_buffers = malloc(tau * sizeof(uint8_t*));
  assert(threads && tasks && qtmp_buffers);

  for (unsigned int i = 0; i < tau; ++i) {
    qtmp_buffers[i] = malloc(MAX_DEPTH * ellhat_bytes);
    assert(qtmp_buffers[i]);
  }

  for (unsigned int i = 0; i < tau; ++i) {
    tasks[i].iv = iv;
    tasks[i].c = c;
    tasks[i].i = i;
    tasks[i].i_delta = i_delta[i];
    tasks[i].ellhat_bytes = ellhat_bytes;
    tasks[i].sd_i_in = bavc_rec.s + sd_offsets[i];
    tasks[i].qtmp = qtmp_buffers[i];
    tasks[i].q = q;
    tasks[i].q_offset = q_offsets[i];
    tasks[i].params = params;
    tasks[i].result_ki = 0;
  }

  for (unsigned int batch_start = 0; batch_start < tau; batch_start += num_threads) {
    unsigned int batch_end = batch_start + num_threads;
    if (batch_end > tau) batch_end = tau;
    unsigned int batch_size = batch_end - batch_start;

    for (unsigned int t = 0; t < batch_size; ++t) {
      pthread_create(&threads[t], NULL, vole_reconstruct_thread, &tasks[batch_start + t]);
    }
    for (unsigned int t = 0; t < batch_size; ++t) {
      pthread_join(threads[t], NULL);
    }
  }

  unsigned int q_idx = 0;
  for (unsigned int i = 0; i < tau; ++i) {
    q_idx += tasks[i].result_ki;
  }

  for (unsigned int i = 0; i < tau; ++i) {
    free(qtmp_buffers[i]);
  }
  free(qtmp_buffers);
  free(tasks);
  free(threads);
  free(sd_offsets);
  free(q_offsets);

#else
  /* Sequential implementation */

  uint8_t* sd   = malloc((1 << k) * lambda_bytes);
  uint8_t* qtmp = malloc(MAX_DEPTH * ellhat_bytes);
  assert(sd);
  assert(qtmp);

  unsigned int q_idx = 0;
  uint8_t* sd_i      = bavc_rec.s;
  for (unsigned int i = 0; i < tau; i++) {
    const unsigned int Ni = bavc_max_node_index(i, tau1, k);

    for (unsigned int j = 0; j < Ni; j++) {
      if (j < i_delta[i]) {
        memcpy(sd + (j ^ i_delta[i]) * lambda_bytes, sd_i + lambda_bytes * j, lambda_bytes);
      } else if (j > i_delta[i]) {
        memcpy(sd + (j ^ i_delta[i]) * lambda_bytes, sd_i + lambda_bytes * (j - 1), lambda_bytes);
      }
    }

    const unsigned int ki = convert_to_vole(iv, sd, true, i, ellhat_bytes, NULL, qtmp, params);

    if (i == 0) {
      memcpy(q[q_idx], qtmp, ellhat_bytes * ki);
      q_idx += ki;
    } else {
      for (unsigned int d = 0; d < ki; ++d, ++q_idx) {
        masked_xor_u8_array(qtmp + d * ellhat_bytes, c + (i - 1) * ellhat_bytes, q[q_idx],
                            (i_delta[i] >> d) & 1, ellhat_bytes);
      }
    }
    sd_i += lambda_bytes * (Ni - 1);
  }

  free(qtmp);
  free(sd);

#endif /* FAEST_PARALLEL_VOLE */

  // ensure 0-padding up to lambda
  for (; q_idx != lambda; ++q_idx) {
    memset(q[q_idx], 0, ellhat_bytes);
  }

  free(bavc_rec.s);
  return true;
}
