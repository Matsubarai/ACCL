#pragma once
/*******************************************************************************
#  Copyright (C) 2022 Xilinx, Inc
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
#
*******************************************************************************/
#include <bits/stdint-uintn.h>
#include <map>
#include <stddef.h>

/** @file constants.hpp */

/** ACCL namespace */
namespace ACCL {
/** Device address type */
typedef uint64_t addr_t;
/** Device value type */
typedef uint32_t val_t;
/** Tag any */
const unsigned int TAG_ANY = 0xFFFFFFFF;
/** Exchange mem offset address */
const addr_t EXCHANGE_MEM_OFFSET_ADDRESS = 0x0;
/** Exchange mem address range */
const addr_t EXCHANGE_MEM_ADDRESS_RANGE = 0x2000;
/** Return code offset */
const addr_t RETCODE_OFFSET = 0x1FFC;
/** hardware id offset */
const addr_t IDCODE_OFFSET = 0x1FF8;
/** Configuration ready offset */
const addr_t CFGRDY_OFFSET = 0x1FF4;

/**
 * Configuration function
 *
 */
enum class cfgFunc {
  reset_periph = 0,
  enable_pkt = 1,
  set_timeout = 2,
  open_port = 3,
  open_con = 4,
  set_stack_type = 5,
  set_max_segment_size = 6
};

/**
 * ACCL operation
 *
 */
enum class operation {
  config = 0,           /**< Set CCLO config */
  copy = 1,             /**< Copy FPGA buffer */
  combine = 2,          /**< Perform reduce operator on FPGA buffers */
  send = 3,             /**< MPI send */
  recv = 4,             /**< MPI recv */
  bcast = 5,            /**< MPI bcast */
  scatter = 6,          /**< MPI scatter */
  gather = 7,           /**< MPI gather */
  reduce = 8,           /**< MPI reduce */
  allgather = 9,        /**< MPI allgather */
  allreduce = 10,       /**< MPI allreduce */
  reduce_scatter = 11,  /**< MPI reduce_scatter */
  ext_stream_krnl = 12, /**< Stream kernel */
  nop = 255             /**< NOP */
};

/**
 * ACCL reduce function
 *
 * Used by  operation::combine, operation::reduce,
 * operation::allreduce and operation::reduce_scatter.
 */
enum class reduceFunction { SUM = 0 };

/**
 * ACCL data types
 *
 */
enum class dataType {
  none,    /**< No datatype, only used internally. */
  int8,    /**< Unsupported datatype, only used internally. */
  float16, /**< float16 */
  float32, /**< float32 */
  float64, /**< float64 */
  int32,   /**< int32 */
  int64    /**< int64 */
};

const std::map<dataType, unsigned int> dataTypeSize = {
    {dataType::none, 0},     {dataType::int8, 8},     {dataType::float16, 16},
    {dataType::float32, 32}, {dataType::float64, 64}, {dataType::int32, 32},
    {dataType::int64, 64}};

enum class streamFlags { NO_STREAM = 0, OP0_STREAM = 1, RES_STREAM = 2 };

inline streamFlags operator|(streamFlags lhs, streamFlags rhs) {
  return static_cast<streamFlags>(static_cast<int>(lhs) |
                                  static_cast<int>(rhs));
}

inline streamFlags &operator|=(streamFlags &lhs, streamFlags rhs) {
  lhs = lhs | rhs;
  return lhs;
}

enum class compressionFlags {
  NO_COMPRESSION = 0,
  OP0_COMPRESSED = 1,
  OP1_COMPRESSED = 2,
  RES_COMPRESSED = 4,
  ETH_COMPRESSED = 8
};

enum class networkProtocol { TCP, UDP, RDMA };

inline compressionFlags operator|(compressionFlags lhs, compressionFlags rhs) {
  return static_cast<compressionFlags>(static_cast<int>(lhs) |
                                       static_cast<int>(rhs));
}

inline compressionFlags &operator|=(compressionFlags &lhs,
                                    compressionFlags rhs) {
  lhs = lhs | rhs;
  return lhs;
}

enum class errorCode {
  COLLECTIVE_OP_SUCCESS = 0,
  DMA_MISMATCH_ERROR = 1 << 0,
  DMA_INTERNAL_ERROR = 1 << 1,
  DMA_DECODE_ERROR = 1 << 2,
  DMA_SLAVE_ERROR = 1 << 3,
  DMA_NOT_OKAY_ERROR = 1 << 4,
  DMA_NOT_END_OF_PACKET_ERROR = 1 << 5,
  DMA_NOT_EXPECTED_BTT_ERROR = 1 << 6,
  DMA_TIMEOUT_ERROR = 1 << 7,
  CONFIG_SWITCH_ERROR = 1 << 8,
  DEQUEUE_BUFFER_TIMEOUT_ERROR = 1 << 9,
  DEQUEUE_BUFFER_SPARE_BUFFER_STATUS_ERROR = 1 << 10,
  RECEIVE_TIMEOUT_ERROR = 1 << 11,
  DEQUEUE_BUFFER_SPARE_BUFFER_DMATAG_MISMATCH = 1 << 12,
  DEQUEUE_BUFFER_SPARE_BUFFER_INDEX_ERROR = 1 << 13,
  COLLECTIVE_NOT_IMPLEMENTED = 1 << 14,
  RECEIVE_OFFCHIP_SPARE_BUFF_ID_NOT_VALID = 1 << 15,
  OPEN_PORT_NOT_SUCCEEDED = 1 << 16,
  OPEN_CON_NOT_SUCCEEDED = 1 << 17,
  DMA_SIZE_ERROR = 1 << 18,
  ARITH_ERROR = 1 << 19,
  PACK_TIMEOUT_STS_ERROR = 1 << 20,
  PACK_SEQ_NUMBER_ERROR = 1 << 21,
  COMPRESSION_ERROR = 1 << 22,
  KRNL_TIMEOUT_STS_ERROR = 1 << 23,
  KRNL_STS_COUNT_ERROR = 1 << 24,
  SEGMENTER_EXPECTED_BTT_ERROR = 1 << 25,
  DMA_TAG_MISMATCH_ERROR = 1 << 26
};

inline errorCode operator|(errorCode lhs, errorCode rhs) {
  return static_cast<errorCode>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

inline errorCode &operator|=(errorCode &lhs, errorCode rhs) {
  lhs = lhs | rhs;
  return lhs;
}

const size_t error_code_bits = 26;

const char *error_code_to_string(errorCode code);
} // namespace ACCL