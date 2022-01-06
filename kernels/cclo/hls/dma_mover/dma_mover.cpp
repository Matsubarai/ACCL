/*******************************************************************************
#  Copyright (C) 2021 Xilinx, Inc
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
# *******************************************************************************/

#include "dma_mover.h"
#include "Axi.h"

using namespace hlslib;

void router_cmd_execute(
    STREAM<router_instruction> &instruction, 
    STREAM<segmenter_cmd> &dma0_read_seg_cmd,
    STREAM<segmenter_cmd> &dma1_read_seg_cmd,
    STREAM<segmenter_cmd> &krnl_in_seg_cmd,
    STREAM<segmenter_cmd> &krnl_out_seg_cmd,
    STREAM<segmenter_cmd> &arith_op0_seg_cmd,
    STREAM<segmenter_cmd> &arith_op1_seg_cmd,
    STREAM<segmenter_cmd> &arith_res_seg_cmd,
    STREAM<segmenter_cmd> &clane0_op_seg_cmd,
    STREAM<segmenter_cmd> &clane0_res_seg_cmd,
    STREAM<segmenter_cmd> &clane1_op_seg_cmd,
    STREAM<segmenter_cmd> &clane1_res_seg_cmd,
    STREAM<segmenter_cmd> &clane2_op_seg_cmd,
    STREAM<segmenter_cmd> &clane2_res_seg_cmd,
    STREAM<ap_uint<32> > &ack_instruction
){
#pragma HLS PIPELINE II=1 style=flp
    router_instruction insn = STREAM_READ(instruction);
    segmenter_cmd cmd;
    bool op0_compress, op1_compress, res_compress;
    bool op0_decompress, op1_decompress, res_decompress;
    bool use_clane0, use_clane1, use_clane2;
    //select final destination
    unsigned int final_dest;
    if(insn.stream_out){
        final_dest = SWITCH_M_EXT_KRNL;
    } else if(insn.eth_out){
        final_dest = SWITCH_M_ETH_TX;
    } else {
        final_dest = SWITCH_M_DMA1_WRITE;
    }
    //precondition the segmenter cmd
    cmd.emit_ack = 0;
    cmd.indeterminate_btt = 0;
    //command segmenters
    if(!insn.use_arithmetic){
        op0_compress = !insn.op0_compressed & insn.res_compressed;
        op0_decompress = insn.op0_compressed & !insn.res_compressed;
        op1_compress = false;
        op1_decompress = false;
        res_compress = false;
        res_decompress = false;
        use_clane0 = op0_compress | op0_decompress;
        use_clane1 = false;
        use_clane2 = false;
        //input from DMA0 read or kernel stream
        cmd.dest = use_clane0 ? SWITCH_M_CLANE0 : final_dest;
        cmd.nwords = insn.op0_compressed ? insn.c_nwords : insn.u_nwords;
        if(insn.stream_in){
            STREAM_WRITE(krnl_in_seg_cmd, cmd);
        } else{
            STREAM_WRITE(dma0_read_seg_cmd, cmd);
        }
        //command clane0 with correct operation
        if(use_clane0){
            //direct clane0 operation
            cmd.dest = op0_compress ? insn.compress_func : insn.decompress_func;
            cmd.nwords = insn.op0_compressed ? insn.c_nwords : insn.u_nwords;
            STREAM_WRITE(clane0_op_seg_cmd, cmd);
            //direct clane0 result to DMA1 write or kernel stream
            cmd.dest = final_dest;
            cmd.nwords = op0_compress ? insn.c_nwords : insn.u_nwords;
            STREAM_WRITE(clane0_res_seg_cmd, cmd);
        }
	} else {
        if(insn.arith_compressed){
            op0_compress = !insn.op0_compressed;
            op0_decompress = false;
            op1_compress = !insn.op1_compressed;
            op1_decompress = false;
            res_compress = false;
            res_decompress = !insn.res_compressed;
        } else{
            op0_compress = false;
            op0_decompress = insn.op0_compressed;
            op1_compress = false;
            op1_decompress = insn.op1_compressed;
            res_compress = insn.res_compressed;
            res_decompress = false;
        }
        use_clane0 = op0_compress | op0_decompress;
        use_clane1 = op1_compress | op1_decompress;
        use_clane2 = res_compress | res_decompress;
        //input from DMA0 read or kernel stream to clane0
        cmd.dest = use_clane0? SWITCH_M_CLANE0 : SWITCH_M_ARITH_OP0;
        cmd.nwords = insn.op0_compressed ? insn.c_nwords : insn.u_nwords;
        if(insn.stream_in){
            STREAM_WRITE(krnl_in_seg_cmd, cmd);
        } else{
            STREAM_WRITE(dma0_read_seg_cmd, cmd);
        }
        //input from DMA1 read to clane1
        cmd.dest = use_clane1 ? SWITCH_M_CLANE1 : SWITCH_M_ARITH_OP1;
        cmd.nwords = insn.op1_compressed ? insn.c_nwords : insn.u_nwords;
        STREAM_WRITE(dma1_read_seg_cmd, cmd);
        //direct clanes to arithmetic
        if(use_clane0){
            cmd.dest = op0_compress ? insn.compress_func : insn.decompress_func;
            cmd.nwords = insn.op0_compressed ? insn.c_nwords : insn.u_nwords;
            STREAM_WRITE(clane0_op_seg_cmd, cmd);
            cmd.dest = SWITCH_M_ARITH_OP0;
            cmd.nwords = op0_compress ? insn.c_nwords : insn.u_nwords;
            STREAM_WRITE(clane0_res_seg_cmd, cmd);
        }
        if(use_clane1){
            cmd.dest = op1_compress ? insn.compress_func : insn.decompress_func;
            cmd.nwords = insn.op1_compressed ? insn.c_nwords : insn.u_nwords;
            STREAM_WRITE(clane1_op_seg_cmd, cmd);
            cmd.dest = SWITCH_M_ARITH_OP1;
            cmd.nwords = op1_compress ? insn.c_nwords : insn.u_nwords;
            STREAM_WRITE(clane1_res_seg_cmd, cmd);
        }
        //set arithmetic function, and direct result to clane2
        cmd.dest = insn.arith_func;
        cmd.nwords = insn.arith_compressed ? insn.c_nwords : insn.u_nwords;
        STREAM_WRITE(arith_op0_seg_cmd, cmd);
        STREAM_WRITE(arith_op1_seg_cmd, cmd);
        cmd.dest = use_clane2 ? SWITCH_M_CLANE2 : final_dest;
        cmd.nwords = insn.arith_compressed ? insn.c_nwords : insn.u_nwords;
        STREAM_WRITE(arith_res_seg_cmd, cmd);
        //direct clane0 result to DMA1 write or kernel stream
        if(use_clane2){
            cmd.dest = res_compress ? insn.compress_func : insn.decompress_func;
            cmd.nwords = insn.arith_compressed ? insn.c_nwords : insn.u_nwords;
            STREAM_WRITE(clane2_op_seg_cmd, cmd);
            cmd.dest = final_dest;
            cmd.nwords = insn.res_compressed ? insn.c_nwords : insn.u_nwords;
            STREAM_WRITE(clane2_res_seg_cmd, cmd);
        }
	}
    //control output kernel stream segmenter if in use
    if(insn.stream_out){
        cmd.dest = insn.krnl_id;
        cmd.nwords = insn.res_compressed ? insn.c_nwords : insn.u_nwords;
        cmd.emit_ack = 1;
        STREAM_WRITE(krnl_out_seg_cmd, cmd);
        STREAM_WRITE(ack_instruction, cmd.nwords);
    }
}

//acknowledge just the segmenter on the kernel stream output
//all the other ones are on some intermediate path so
//their acknowledge is redundant
void router_ack_execute(
    STREAM<ap_uint<32> > &instruction, 
    STREAM<ap_uint<32> > &krnl_out_seg_ack,
    STREAM<ap_uint<32> > &error 
){
#pragma HLS PIPELINE II=1 style=flp
    if(!STREAM_IS_EMPTY(instruction)){
        ap_uint<32> expected = STREAM_READ(instruction);
        ap_uint<32> received = STREAM_READ(krnl_out_seg_ack);
        if(expected != received){
            STREAM_WRITE(error, SEGMENTER_EXPECTED_BTT_ERROR);
        } else {
            STREAM_WRITE(error, NO_ERROR);
        }
    }
}


// start a DMA operations on a specific channel
// compute number of bytes from compression info
// break up a large transfer into DMA_MAX_BTT chunks
void dma_cmd_execute(
    STREAM<datamover_instruction> &instruction,
    STREAM<ap_uint<104> > &dma_cmd_channel,
    STREAM<ap_uint<32> > &ack_instruction
) {
#pragma HLS PIPELINE II=1 style=flp
    ap_uint<4> tag = 0;
    ap_uint<32> ncommands = 0;
    unsigned int btt;
    axi::Command<64, 23> dma_cmd;
    datamover_ack_instruction ack_cmd;
    datamover_instruction instr;

    if(!STREAM_IS_EMPTY(instruction)){
        instr = STREAM_READ(instruction);
        while(instr.total_bytes > 0){
            btt = (instr.total_bytes > DMA_MAX_BTT) ? DMA_MAX_BTT : instr.total_bytes;
            dma_cmd.length = btt;
            dma_cmd.address = instr.addr;
            dma_cmd.tag = tag;
            STREAM_WRITE(dma_cmd_channel, dma_cmd);
            //update state
            instr.total_bytes -= btt;
            tag++;
            ncommands++;
        }
        STREAM_WRITE(ack_instruction, ncommands);
    }
}

// check DMA status, record any errors and forward
void dma_ack_execute(
    STREAM<ap_uint<32> > &instruction,
    STREAM<ap_uint<32> > &dma_sts_channel,
    STREAM<ap_uint<32> > &error
) {
#pragma HLS PIPELINE II=1 style=flp
    ap_uint<32> ret = NO_ERROR;
    ap_uint<32> ncmds;
    axi::Status status;
    unsigned int tag = 0;
    if(!STREAM_IS_EMPTY(instruction)){
        ncmds = STREAM_READ(instruction);
        while(ncmds > 0){
            status = axi::Status(STREAM_READ(dma_sts_channel));
            if(status.tag != tag) {
                ret = ret | DMA_TAG_MISMATCH_ERROR;
            }
            if(status.internalError) {
                ret = ret | DMA_INTERNAL_ERROR;
            }
            if(status.decodeError) {
                ret = ret | DMA_DECODE_ERROR;
            }
            if(status.slaveError) {
                ret = ret | DMA_SLAVE_ERROR;
            }
            if(!status.okay) {
                ret = ret | DMA_NOT_OKAY_ERROR;
            }
            ncmds--;
            tag++;
        }
        STREAM_WRITE(error, ret);
    }
}

void eth_cmd_execute(
    STREAM<packetizer_instruction> &instruction,
    STREAM<eth_header> &eth_cmd_channel,
    STREAM<ap_uint<32> > &ack_instruction,
    unsigned int max_segment_len
) {
#pragma HLS PIPELINE II=1 style=flp
    unsigned int seg_len, sequence_number;
    if(!STREAM_IS_EMPTY(instruction)){
        packetizer_instruction insn = STREAM_READ(instruction);
        sequence_number = insn.seqn;
        while(insn.len > 0){
            //NOTE: to make sure we don't get into trouble with ragged ends on streams,
            //max_segment_len should be a multiple of the datapath width (64B)
            //this shouldn't be a problem in practice
            seg_len = (insn.len > max_segment_len) ? max_segment_len : insn.len;
            // prepare header arguments
            eth_header pkt_cmd;
            // enqueue message headers
            pkt_cmd.count = seg_len;
            pkt_cmd.tag = insn.mpi_tag;
            pkt_cmd.src = insn.src_rank;
            pkt_cmd.seqn = sequence_number;
            pkt_cmd.strm = insn.to_stream ? insn.mpi_tag : 0;
            pkt_cmd.dst = insn.dst_sess_id;
            STREAM_WRITE(eth_cmd_channel, pkt_cmd);
            STREAM_WRITE(ack_instruction, sequence_number);
            // update state
            insn.len -= seg_len;
            sequence_number++;
        }
    }
}

// check that packetizer has finished processing every packet it was supposed to.
void eth_ack_execute(
    STREAM<ap_uint<32> > &instruction,
    STREAM<ap_uint<32> > &eth_ack_channel,
    STREAM<ap_uint<32> > &error
) {
#pragma HLS PIPELINE II=1 style=flp
    unsigned int expected_seq_number, ack_seq_num;
    if(!STREAM_IS_EMPTY(instruction)){
        expected_seq_number = STREAM_READ(instruction);
        ack_seq_num = STREAM_READ(eth_ack_channel);
        if(expected_seq_number == ack_seq_num) {
            STREAM_WRITE(error, NO_ERROR);
        } else {
            STREAM_WRITE(error, PACK_SEQ_NUMBER_ERROR);
        }
    }
}

void instruction_fetch(
    STREAM<ap_axiu<32,0,0,0> > &cmd,
    STREAM<move_instruction> &instruction
){
#pragma HLS PIPELINE II=1 style=flp
    ap_uint<32> tmp;
    move_instruction ret;

    tmp = (STREAM_READ(cmd)).data;
    ret.op0_opcode = tmp(2,0);
    ret.op1_opcode = tmp(5,3);
    ret.res_opcode = tmp(8,6);
    ap_uint<1> remote_flags = tmp(9,9);
    ret.res_is_remote = (remote_flags == RES_REMOTE);
    ap_uint<3> compression_flags = tmp(12,10);
    ret.op0_is_compressed = (compression_flags & OP0_COMPRESSED) != 0;
    ret.op1_is_compressed = (compression_flags & OP1_COMPRESSED) != 0;
    ret.res_is_compressed = (compression_flags & RES_COMPRESSED) != 0;
    ret.func_id = tmp(16,13);
    
    ret.count = (STREAM_READ(cmd)).data;

    //get arith config offset
    ret.arcfg_offset = (STREAM_READ(cmd)).data;

    //get addr for op0, or equivalents
    if(ret.op0_opcode == MOVE_IMMEDIATE){
        ret.op0_addr(31,0) = (STREAM_READ(cmd)).data;
        ret.op0_addr(63,32) = (STREAM_READ(cmd)).data;
    } else if(ret.op0_opcode == MOVE_STRIDE){
        ret.op0_stride = (STREAM_READ(cmd)).data;
    }
    //get addr for op1, or equivalents
    if(ret.op1_opcode == MOVE_IMMEDIATE){
        ret.op1_addr(31,0) = (STREAM_READ(cmd)).data;
        ret.op1_addr(63,32) = (STREAM_READ(cmd)).data;
    } else if(ret.op1_opcode == MOVE_ON_RECV){
        ret.rx_src = (STREAM_READ(cmd)).data;
        ret.rx_tag = (STREAM_READ(cmd)).data;
    } else if(ret.op1_opcode == MOVE_STRIDE){
        ret.op1_stride = (STREAM_READ(cmd)).data;
    }
    //get addr for res, or equivalents
    if(ret.res_opcode == MOVE_IMMEDIATE){
        ret.res_addr(31,0) = (STREAM_READ(cmd)).data;
        ret.res_addr(63,32) = (STREAM_READ(cmd)).data;
    } else if(ret.res_opcode == MOVE_STRIDE){
        ret.res_stride = (STREAM_READ(cmd)).data;
    }
    //get send related stuff, if result is remote or stream
    if(ret.res_is_remote || ret.res_opcode == MOVE_STREAM){
        ret.mpi_tag = (STREAM_READ(cmd)).data;
    }
    if(ret.res_is_remote){
        ret.comm_offset = (STREAM_READ(cmd)).data;
        ret.dst_rank = (STREAM_READ(cmd)).data;
    }

    STREAM_WRITE(instruction, ret);
}

unsigned int get_len(unsigned int count, unsigned int elem_ratio_log, unsigned int elem_bytes){
#pragma HLS INLINE
    return (count >> elem_ratio_log) * elem_bytes;
}


void instruction_decode(
    STREAM<move_instruction> &instruction,
    STREAM<datamover_instruction> &op0_dm_insn,
    STREAM<datamover_instruction> &op1_dm_insn,
    STREAM<datamover_instruction> &res_dm_insn,
    STREAM<packetizer_instruction> &eth_insn,
    STREAM<router_instruction> &router_insn,
    STREAM<move_ack_instruction> &err_instruction,
    STREAM<rxbuf_signature> &rxbuf_req,
    STREAM<rxbuf_seek_result> &rxbuf_ack,
    unsigned int * exchange_mem,
    unsigned int max_segment_len
){
#pragma HLS PIPELINE II=1 style=flp
    move_instruction insn = STREAM_READ(instruction);
    unsigned int src, seqn, session;
    datamover_instruction dm0_rd, dm1_rd, dm1_wr;
    packetizer_instruction pkt_wr;
    router_instruction rtr;
    //instruction for checking execution units completion
    move_ack_instruction ack_insn;
    ack_insn.release_rxbuf = false;
    ack_insn.check_strm_tx = false;
    ack_insn.check_dma0_rx = false;
    ack_insn.check_dma1_rx = false;
    ack_insn.check_dma1_tx = false;
    ack_insn.check_eth_tx = false;
    //NOTE: if count is zero, we do all address manipulation but do not issue
    //actual commands to any execution units; this effectively creates an initialization
    //instruction, like a NOP with side-effects in the address registers
    bool dry_run = (insn.count == 0);
    //get communicator if targeting a remote node
    if(insn.res_is_remote && !dry_run){
        pkt_wr.src_rank = *(exchange_mem + insn.comm_offset + COMM_LOCAL_RANK_OFFSET);
        pkt_wr.seqn = *(exchange_mem + insn.comm_offset + COMM_RANKS_OFFSET + (insn.dst_rank * RANK_SIZE) + RANK_OUTBOUND_SEQ_OFFSET);
        pkt_wr.dst_sess_id = *(exchange_mem + insn.comm_offset + COMM_RANKS_OFFSET + (insn.dst_rank * RANK_SIZE) + RANK_SESSION_OFFSET);
        *(exchange_mem + insn.comm_offset + COMM_RANKS_OFFSET + (insn.dst_rank * RANK_SIZE) + RANK_OUTBOUND_SEQ_OFFSET) = pkt_wr.seqn+1;
    }
    //get arithmetic config unless we have already cached it
    static datapath_arith_config arcfg;
    static bool arcfg_cached = false;
    static unsigned int prev_arcfg_offset;
    if(!arcfg_cached || (insn.arcfg_offset != prev_arcfg_offset && !dry_run)){
        arcfg = *((datapath_arith_config*)(exchange_mem + insn.arcfg_offset));
        arcfg_cached = true;
        prev_arcfg_offset = insn.arcfg_offset;
    }
    //compute total transfer bytes for compressed and uncompressed scenarios
    unsigned int total_bytes_uncompressed = get_len(insn.count, 0, arcfg.uncompressed_elem_bytes);
    unsigned int total_bytes_compressed = get_len(insn.count, arcfg.elem_ratio_log, arcfg.compressed_elem_bytes);
    //service requests
    rtr.c_nwords = (total_bytes_compressed+63) / 64;
    rtr.u_nwords = (total_bytes_uncompressed+63) / 64;
    rtr.stream_in = (insn.op0_opcode == MOVE_STREAM);
    rtr.stream_out = (insn.res_opcode == MOVE_STREAM);
    rtr.eth_out = (insn.res_opcode != MOVE_NONE) & insn.res_is_remote;
    rtr.op0_compressed = insn.op0_is_compressed;
    rtr.op1_compressed = insn.op1_is_compressed;
    rtr.res_compressed = insn.res_is_compressed;
    rtr.use_arithmetic = (insn.op0_opcode != MOVE_NONE) & (insn.op1_opcode != MOVE_NONE);
    rtr.arith_compressed = arcfg.arith_is_compressed;
    rtr.arith_func = arcfg.arith_tdest[insn.func_id];
    rtr.compress_func = arcfg.compressor_tdest;
    rtr.decompress_func = arcfg.decompressor_tdest;
    rtr.krnl_id = insn.mpi_tag;
    if(!dry_run){
        STREAM_WRITE(router_insn, rtr);
        ack_insn.check_strm_tx = rtr.stream_out;
    }
    //issue commands to datamovers, if required
    //DM0 read channel corresponding to OP0
    static datamover_instruction prev_dm0_rd;
    if((insn.op0_opcode != MOVE_NONE) & (insn.op0_opcode != MOVE_STREAM)){
        dm0_rd.total_bytes = insn.op0_is_compressed ? total_bytes_compressed : total_bytes_uncompressed;
        switch(insn.op0_opcode){
            //add options here - reuse, increment, etc
            case MOVE_IMMEDIATE:
                dm0_rd.addr = insn.op0_addr;
                break;
            case MOVE_INCREMENT:
                dm0_rd.addr = prev_dm0_rd.addr + prev_dm0_rd.total_bytes;
                break;
            case MOVE_REPEAT:
                dm0_rd.addr = prev_dm0_rd.addr;
                break;
            case MOVE_STRIDE:
                dm0_rd.addr = prev_dm0_rd.addr + 
                    (insn.op0_is_compressed ? 
                        get_len(insn.op0_stride, arcfg.elem_ratio_log, arcfg.compressed_elem_bytes) : 
                        get_len(insn.op0_stride, 0, arcfg.uncompressed_elem_bytes)    );
                break;
            default:
                dm0_rd.addr = insn.op0_addr;
                break;
        }
        if(!dry_run){
            STREAM_WRITE(op0_dm_insn, dm0_rd);
            ack_insn.check_dma0_rx = true;
        }
        prev_dm0_rd = dm0_rd;
    }
    //DM1 read channel corresponding to OP1
    static datamover_instruction prev_dm1_rd;
    rxbuf_seek_result seek_res;
    unsigned int inbound_seqn;
    if(insn.op1_opcode != MOVE_NONE){
        dm1_rd.total_bytes = insn.op1_is_compressed ? total_bytes_compressed : total_bytes_uncompressed;
        switch(insn.op1_opcode){
            //add options here - reuse, increment, etc
            case MOVE_IMMEDIATE:
                dm1_rd.addr = insn.op1_addr;
                break;
            case MOVE_INCREMENT:
                dm1_rd.addr = prev_dm1_rd.addr + prev_dm1_rd.total_bytes;
                break;
            case MOVE_REPEAT:
                dm1_rd.addr = prev_dm1_rd.addr;
                break;
            case MOVE_STRIDE:
                dm1_rd.addr = prev_dm1_rd.addr + 
                    (insn.op1_is_compressed ? 
                        get_len(insn.op1_stride, arcfg.elem_ratio_log, arcfg.compressed_elem_bytes) : 
                        get_len(insn.op1_stride, 0, arcfg.uncompressed_elem_bytes)    );
                break;
            case MOVE_ON_RECV:
                //get expected sequence number for the source rank by incrementing previous sequence number
                inbound_seqn = 1 + *(exchange_mem + insn.comm_offset + COMM_RANKS_OFFSET + (insn.rx_src * RANK_SIZE) + RANK_INBOUND_SEQ_OFFSET);
                //emit rx seek queries until one returns true
                do{
                    STREAM_WRITE(rxbuf_req, ((rxbuf_signature){.tag=insn.rx_tag, .len=dm1_rd.total_bytes, .src=insn.rx_src, .seqn=inbound_seqn}));
                    seek_res = STREAM_READ(rxbuf_ack);
                }while(!seek_res.valid);
                dm1_rd.addr = seek_res.addr;
                //update expected sequence number
                *(exchange_mem + insn.comm_offset + COMM_RANKS_OFFSET + (insn.rx_src * RANK_SIZE) + RANK_INBOUND_SEQ_OFFSET) = inbound_seqn;
                //instruct to release this buffer once the DMA movement is complete
                ack_insn.release_rxbuf = true;
                ack_insn.rxbuf_idx = seek_res.index;
                break;
            default:
                dm1_rd.addr = insn.op1_addr;
                break;
        }
        if(!dry_run){
            STREAM_WRITE(op1_dm_insn, dm1_rd);
            ack_insn.check_dma1_rx = true;
        }
        prev_dm1_rd = dm1_rd;
    }
    //DM1 write channel corresponding to RES
    static datamover_instruction prev_dm1_wr;
    if(insn.res_opcode != MOVE_NONE){
        if(insn.res_is_remote){
            pkt_wr.len = insn.res_is_compressed ? total_bytes_compressed : total_bytes_uncompressed;
            pkt_wr.mpi_tag = insn.mpi_tag;
            pkt_wr.to_stream = (insn.res_opcode == MOVE_STREAM);
            if(!dry_run){
                STREAM_WRITE(eth_insn, pkt_wr);
                ack_insn.check_eth_tx = true;
            }
        } else if(!(insn.res_opcode == MOVE_STREAM)){
            dm1_wr.total_bytes = insn.res_is_compressed ? total_bytes_compressed : total_bytes_uncompressed;
            switch(insn.res_opcode){
                case MOVE_IMMEDIATE:
                    dm1_wr.addr = insn.res_addr;
                    break;
                case MOVE_INCREMENT:
                    dm1_wr.addr = prev_dm1_wr.addr + prev_dm1_wr.total_bytes;
                    break;
                case MOVE_REPEAT:
                    dm1_wr.addr = prev_dm1_wr.addr;
                    break;
                case MOVE_STRIDE:
                    dm1_wr.addr = prev_dm1_wr.addr + 
                        (insn.res_is_compressed ? 
                            get_len(insn.res_stride, arcfg.elem_ratio_log, arcfg.compressed_elem_bytes) : 
                            get_len(insn.res_stride, 0, arcfg.uncompressed_elem_bytes)    );
                    break;
                default:
                    dm1_wr.addr = insn.res_addr;
                    break;
            }
            if(!dry_run){
                STREAM_WRITE(res_dm_insn, dm1_wr);
                ack_insn.check_dma1_tx = true;
            }
        }
    }

    //emit command to instruction acknowledge engine 
    STREAM_WRITE(err_instruction, ack_insn);
}

void instruction_retire(
    STREAM<move_ack_instruction> &instruction,
    STREAM<ap_uint<32> > &dma0_rx_err,
    STREAM<ap_uint<32> > &dma1_rx_err,
    STREAM<ap_uint<32> > &rxbuf_release_req,
    STREAM<ap_uint<32> > &dma1_tx_err,
    STREAM<ap_uint<32> > &eth_tx_err,
    STREAM<ap_uint<32> > &strm_tx_err,
    STREAM<ap_axiu<32,0,0,0> > &error
){
#pragma HLS PIPELINE II=1 style=flp
    move_ack_instruction insn = STREAM_READ(instruction);
    ap_axiu<32,0,0,0> err;
    err.last = 1;
    err.data = NO_ERROR;
    if(insn.check_dma0_rx) err.data |= STREAM_READ(dma0_rx_err);
    if(insn.check_dma1_rx){
        err.data |= STREAM_READ(dma1_rx_err);
        if(insn.release_rxbuf && err.data == NO_ERROR){
            STREAM_WRITE(rxbuf_release_req, insn.rxbuf_idx);
        }
    }
    if(insn.check_dma1_tx) err.data |= STREAM_READ(dma1_tx_err);
    if(insn.check_eth_tx) err.data |= STREAM_READ(eth_tx_err);
    if(insn.check_strm_tx) err.data |= STREAM_READ(strm_tx_err);
    STREAM_WRITE(error, err);
}

void dma_mover(
    //interfaces to processor
    unsigned int * exchange_mem,
    unsigned int max_segment_len,
    STREAM<ap_axiu<32,0,0,0> > &command,
    STREAM<ap_axiu<32,0,0,0> > &error,
    //interfaces to rx buffer seek offload
    STREAM<rxbuf_signature> &rxbuf_req,
    STREAM<rxbuf_seek_result> &rxbuf_ack,
    STREAM<ap_uint<32> > &rxbuf_release_req,
    //interfaces to data movement engines
    STREAM<ap_uint<104> > &dma0_read_cmd,
    STREAM<ap_uint<104> > &dma1_read_cmd,
    STREAM<ap_uint<104> > &dma1_write_cmd,
    STREAM<ap_uint<32>> &dma0_read_sts,
    STREAM<ap_uint<32>> &dma1_read_sts,
    STREAM<ap_uint<32>> &dma1_write_sts,
    STREAM<eth_header> &eth_cmd,
    STREAM<ap_uint<32> > &eth_sts,
    //interfaces to segmenters in the routing path
    STREAM<segmenter_cmd> &dma0_read_seg_cmd,
    STREAM<segmenter_cmd> &dma1_read_seg_cmd,
    STREAM<segmenter_cmd> &krnl_in_seg_cmd,
    STREAM<segmenter_cmd> &krnl_out_seg_cmd,
    STREAM<segmenter_cmd> &arith_op0_seg_cmd,
    STREAM<segmenter_cmd> &arith_op1_seg_cmd,
    STREAM<segmenter_cmd> &arith_res_seg_cmd,
    STREAM<segmenter_cmd> &clane0_op_seg_cmd,
    STREAM<segmenter_cmd> &clane0_res_seg_cmd,
    STREAM<segmenter_cmd> &clane1_op_seg_cmd,
    STREAM<segmenter_cmd> &clane1_res_seg_cmd,
    STREAM<segmenter_cmd> &clane2_op_seg_cmd,
    STREAM<segmenter_cmd> &clane2_res_seg_cmd,
    STREAM<ap_uint<32> > &krnl_out_seg_sts
){
#pragma HLS INTERFACE axis port=command
#pragma HLS INTERFACE axis port=error
#pragma HLS INTERFACE axis port=rxbuf_req
#pragma HLS INTERFACE axis port=rxbuf_ack
#pragma HLS INTERFACE axis port=rxbuf_release_req
#pragma HLS INTERFACE axis port=dma0_read_cmd
#pragma HLS INTERFACE axis port=dma1_read_cmd
#pragma HLS INTERFACE axis port=dma1_write_cmd
#pragma HLS INTERFACE axis port=dma0_read_sts
#pragma HLS INTERFACE axis port=dma1_read_sts
#pragma HLS INTERFACE axis port=dma1_write_sts
#pragma HLS INTERFACE axis port=eth_cmd
#pragma HLS INTERFACE axis port=eth_sts
#pragma HLS INTERFACE axis port=dma0_read_seg_cmd
#pragma HLS INTERFACE axis port=dma1_read_seg_cmd
#pragma HLS INTERFACE axis port=krnl_in_seg_cmd
#pragma HLS INTERFACE axis port=krnl_out_seg_cmd
#pragma HLS INTERFACE axis port=arith_op0_seg_cmd
#pragma HLS INTERFACE axis port=arith_op1_seg_cmd
#pragma HLS INTERFACE axis port=arith_res_seg_cmd
#pragma HLS INTERFACE axis port=clane0_op_seg_cmd
#pragma HLS INTERFACE axis port=clane0_res_seg_cmd
#pragma HLS INTERFACE axis port=clane1_op_seg_cmd
#pragma HLS INTERFACE axis port=clane1_res_seg_cmd
#pragma HLS INTERFACE axis port=clane2_op_seg_cmd
#pragma HLS INTERFACE axis port=clane2_res_seg_cmd
#pragma HLS INTERFACE axis port=krnl_out_seg_sts
#pragma HLS INTERFACE m_axi port=echange_mem offset=slave
#pragma HLS INTERFACE s_axilite port=max_segment_len
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS DATAFLOW disable_start_propagation
    STREAM<move_instruction> instruction;

    STREAM<datamover_instruction> dma0_read_insn;
    STREAM<datamover_instruction> dma1_read_insn;
    STREAM<datamover_instruction> dma1_write_insn;
    STREAM<packetizer_instruction> eth_insn;
    STREAM<router_instruction> router_insn;
    STREAM<move_ack_instruction> err_instruction;

    STREAM<ap_uint<32> > dma0_read_ack_insn;
    STREAM<ap_uint<32> > dma1_read_ack_insn;
    STREAM<ap_uint<32> > dma1_write_ack_insn;

    STREAM<ap_uint<32> > dma0_read_error;
    STREAM<ap_uint<32> > dma1_read_error;
    STREAM<ap_uint<32> > dma1_write_error;

    STREAM<ap_uint<32> > eth_tx_ack_instruction;
    STREAM<ap_uint<32> > eth_tx_error;

    STREAM<ap_uint<32> > strm_tx_ack_instruction;
    STREAM<ap_uint<32> > strm_tx_error;

    instruction_fetch(command, instruction);

    instruction_decode(
        instruction,
        dma0_read_insn,
        dma1_read_insn,
        dma1_write_insn,
        eth_insn,
        router_insn,
        err_instruction,
        rxbuf_req,
        rxbuf_ack,
        exchange_mem,
        max_segment_len
    );

    dma_cmd_execute(dma0_read_insn, dma0_read_cmd, dma0_read_ack_insn);
    dma_cmd_execute(dma1_read_insn, dma1_read_cmd, dma1_read_ack_insn);
    dma_cmd_execute(dma1_write_insn, dma1_write_cmd, dma1_write_ack_insn);
    eth_cmd_execute(eth_insn, eth_cmd, eth_tx_ack_instruction, max_segment_len);
    router_cmd_execute(
        router_insn, 
        dma0_read_seg_cmd,
        dma1_read_seg_cmd,
        krnl_in_seg_cmd,
        krnl_out_seg_cmd,
        arith_op0_seg_cmd,
        arith_op1_seg_cmd,
        arith_res_seg_cmd,
        clane0_op_seg_cmd,
        clane0_res_seg_cmd,
        clane1_op_seg_cmd,
        clane1_res_seg_cmd,
        clane2_op_seg_cmd,
        clane2_res_seg_cmd,
        strm_tx_ack_instruction
    );

    dma_ack_execute(dma0_read_ack_insn, dma0_read_sts, dma0_read_error);
    dma_ack_execute(dma1_read_ack_insn, dma1_read_sts, dma1_read_error);
    dma_ack_execute(dma1_write_ack_insn, dma1_write_sts, dma1_write_error);
    eth_ack_execute(eth_tx_ack_instruction, eth_sts, eth_tx_error);
    router_ack_execute(strm_tx_ack_instruction, krnl_out_seg_sts, strm_tx_error);

    instruction_retire(
        err_instruction,
        dma0_read_error,
        dma1_read_error,
        rxbuf_release_req,
        dma1_write_error,
        eth_tx_error,
        strm_tx_error,
        error
    );

}



