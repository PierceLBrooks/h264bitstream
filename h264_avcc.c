#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "h264_avcc.h"
#include "bs.h"
#include "h264_stream.h"

avcc_t* avcc_new()
{
  avcc_t* avcc = (avcc_t*)calloc(1, sizeof(avcc_t));
  avcc->sps_table = NULL;
  avcc->pps_table = NULL;
  return avcc;
}

void avcc_free(avcc_t* avcc)
{
  if (avcc->sps_table != NULL) { free(avcc->sps_table); }
  if (avcc->pps_table != NULL) { free(avcc->pps_table); }
  free(avcc);
}

int read_avcc(avcc_t* avcc, h264_stream_t* h, bs_t* b)
{
  avcc->configurationVersion = bs_read_u8(b);
  avcc->AVCProfileIndication = bs_read_u8(b);
  avcc->profile_compatibility = bs_read_u8(b);
  avcc->AVCLevelIndication = bs_read_u8(b);
  /* int reserved = */ bs_read_u(b, 6); // '111111'b;
  avcc->lengthSizeMinusOne = bs_read_u(b, 2);
  /* int reserved = */ bs_read_u(b, 3); // '111'b;

  avcc->numOfSequenceParameterSets = bs_read_u(b, 5);
  avcc->sps_table = (sps_t**)calloc(avcc->numOfSequenceParameterSets, sizeof(sps_t*));
  for (int i = 0; i < avcc->numOfSequenceParameterSets; i++)
  {
    int sequenceParameterSetLength = bs_read_u(b, 16);
    int len = sequenceParameterSetLength;
    uint8_t* buf = (uint8_t*)malloc(len);
    len = bs_read_bytes(b, buf, len);
    int rc = read_nal_unit(h, buf, len);
    free(buf);
    if (h->nal->nal_unit_type != NAL_UNIT_TYPE_SPS) { continue; } // TODO report errors
    if (rc < 0) { continue; }
    avcc->sps_table[i] = h->sps; // TODO copy data?
  }

  avcc->numOfPictureParameterSets = bs_read_u(b, 8);
  avcc->pps_table = (pps_t**)calloc(avcc->numOfPictureParameterSets, sizeof(pps_t*));
  for (int i = 0; i < avcc->numOfPictureParameterSets; i++)
  {
    int pictureParameterSetLength = bs_read_u(b, 16);
    int len = pictureParameterSetLength;
    uint8_t* buf = (uint8_t*)malloc(len);
    len = bs_read_bytes(b, buf, len);
    int rc = read_nal_unit(h, buf, len);
    free(buf);
    if (h->nal->nal_unit_type != NAL_UNIT_TYPE_PPS) { continue; } // TODO report errors
    if (rc < 0) { continue; }
    avcc->pps_table[i] = h->pps; // TODO copy data?
  }

  if (bs_overrun(b)) { return -1; }
  return bs_pos(b);
}


int write_avcc(avcc_t* avcc, h264_stream_t* h, bs_t* b)
{
  bs_write_u8(b, 1); // configurationVersion = 1;
  bs_write_u8(b, avcc->AVCProfileIndication);
  bs_write_u8(b, avcc->profile_compatibility);
  bs_write_u8(b, avcc->AVCLevelIndication);
  bs_write_u(b, 6, 0x3F); // reserved = '111111'b;
  bs_write_u(b, 2, avcc->lengthSizeMinusOne);
  bs_write_u(b, 3, 0x07); // reserved = '111'b;

  bs_write_u(b, 5, avcc->numOfSequenceParameterSets);
  for (int i = 0; i < avcc->numOfSequenceParameterSets; i++)
  {
    int max_len = 1024; // FIXME
    uint8_t* buf = (uint8_t*)malloc(max_len);
    h->nal->nal_ref_idc = 3; // NAL_REF_IDC_PRIORITY_HIGHEST;
    h->nal->nal_unit_type = NAL_UNIT_TYPE_SPS;
    h->sps = avcc->sps_table[i];
    int len = write_nal_unit(h, buf, max_len);
    if (len < 0) { free(buf); continue; } // TODO report errors
    int sequenceParameterSetLength = len;
    bs_write_u(b, 16, sequenceParameterSetLength);
    bs_write_bytes(b, buf, len);
    free(buf);
  }

  bs_write_u(b, 8, avcc->numOfPictureParameterSets);
  for (int i = 0; i < avcc->numOfPictureParameterSets; i++)
  {
    int max_len = 1024; // FIXME
    uint8_t* buf = (uint8_t*)malloc(max_len);
    h->nal->nal_ref_idc = 3; // NAL_REF_IDC_PRIORITY_HIGHEST;
    h->nal->nal_unit_type = NAL_UNIT_TYPE_PPS;
    h->pps = avcc->pps_table[i];
    int len = write_nal_unit(h, buf, max_len);
    if (len < 0) { free(buf); continue; } // TODO report errors
    int pictureParameterSetLength = len;
    bs_write_u(b, 16, pictureParameterSetLength);
    bs_write_bytes(b, buf, len);
    free(buf);
  }

  if (bs_overrun(b)) { return -1; }
  return bs_pos(b);
}

void debug_avcc(avcc_t* avcc)
{
  printf("======= AVC Decoder Configuration Record =======\n");
  printf(" configurationVersion: %d\n", avcc->configurationVersion );
  printf(" AVCProfileIndication: %d\n", avcc->AVCProfileIndication );
  printf(" profile_compatibility: %d\n", avcc->profile_compatibility );
  printf(" AVCLevelIndication: %d\n", avcc->AVCLevelIndication );
  printf(" lengthSizeMinusOne: %d\n", avcc->lengthSizeMinusOne );

  printf("\n");
  printf(" numOfSequenceParameterSets: %d\n", avcc->numOfSequenceParameterSets );
  for (int i = 0; i < avcc->numOfSequenceParameterSets; i++)
  {
    //printf(" sequenceParameterSetLength\n", avcc->sequenceParameterSetLength );
    if (avcc->sps_table[i] == NULL) { printf(" null sps\n"); continue; }
    debug_sps(avcc->sps_table[i]);
  }

  printf("\n");
  printf(" numOfPictureParameterSets: %d\n", avcc->numOfPictureParameterSets );
  for (int i = 0; i < avcc->numOfPictureParameterSets; i++)
  {
    //printf(" pictureParameterSetLength\n", avcc->pictureParameterSetLength );
    if (avcc->pps_table[i] == NULL) { printf(" null pps\n"); continue; }
    debug_pps(avcc->pps_table[i]);
  }
}

void debug_sps(sps_t* sps)
{
    int i;

    printf("sps->profile_idc: %d \n", sps->profile_idc);
    printf("sps->constraint_set0_flag: %d \n", sps->constraint_set0_flag); 
    printf("sps->constraint_set1_flag: %d \n", sps->constraint_set1_flag); 
    printf("sps->constraint_set2_flag: %d \n", sps->constraint_set2_flag); 
    printf("sps->constraint_set3_flag: %d \n", sps->constraint_set3_flag); 
    printf("sps->constraint_set4_flag: %d \n", sps->constraint_set4_flag); 
    printf("sps->constraint_set5_flag: %d \n", sps->constraint_set5_flag); 
    printf("sps->level_idc: %d \n", sps->level_idc);
    printf("sps->seq_parameter_set_id: %d \n", sps->seq_parameter_set_id); 

    if( sps->profile_idc == 100 || sps->profile_idc == 110 ||
        sps->profile_idc == 122 || sps->profile_idc == 244 ||
        sps->profile_idc == 44 || sps->profile_idc == 83 ||
        sps->profile_idc == 86 || sps->profile_idc == 118 ||
        sps->profile_idc == 128 || sps->profile_idc == 138 ||
        sps->profile_idc == 139 || sps->profile_idc == 134
       )
    {
        printf("sps->chroma_format_idc: %d \n", sps->chroma_format_idc); 
        if( sps->chroma_format_idc == 3 )
        {
            printf("sps->residual_colour_transform_flag: %d \n", sps->residual_colour_transform_flag); 
        }
        printf("sps->bit_depth_luma_minus8: %d \n", sps->bit_depth_luma_minus8); 
        printf("sps->bit_depth_chroma_minus8: %d \n", sps->bit_depth_chroma_minus8); 
        printf("sps->qpprime_y_zero_transform_bypass_flag: %d \n", sps->qpprime_y_zero_transform_bypass_flag); 
        printf("sps->seq_scaling_matrix_present_flag: %d \n", sps->seq_scaling_matrix_present_flag);
        if( sps->seq_scaling_matrix_present_flag )
        {
            for( i = 0; i < 8; i++ )
            {
                printf("sps->seq_scaling_list_present_flag[ i ]: %d \n", sps->seq_scaling_list_present_flag[ i ]); 
            }
        }
    }
    printf("sps->log2_max_frame_num_minus4: %d \n", sps->log2_max_frame_num_minus4); 
    printf("sps->pic_order_cnt_type: %d \n", sps->pic_order_cnt_type); 
    if( sps->pic_order_cnt_type == 0 )
    {
        printf("sps->log2_max_pic_order_cnt_lsb_minus4: %d \n", sps->log2_max_pic_order_cnt_lsb_minus4); 
    }
    else if( sps->pic_order_cnt_type == 1 )
    {
        printf("sps->delta_pic_order_always_zero_flag: %d \n", sps->delta_pic_order_always_zero_flag); 
        printf("sps->offset_for_non_ref_pic: %d \n", sps->offset_for_non_ref_pic); 
        printf("sps->offset_for_top_to_bottom_field: %d \n", sps->offset_for_top_to_bottom_field); 
        printf("sps->num_ref_frames_in_pic_order_cnt_cycle: %d \n", sps->num_ref_frames_in_pic_order_cnt_cycle); 
        for( i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++ )
        {
            printf("sps->offset_for_ref_frame[ i ]: %d \n", sps->offset_for_ref_frame[ i ]); 
        }
    }
    printf("sps->num_ref_frames: %d \n", sps->num_ref_frames); 
    printf("sps->gaps_in_frame_num_value_allowed_flag: %d \n", sps->gaps_in_frame_num_value_allowed_flag); 
    printf("sps->pic_width_in_mbs_minus1: %d \n", sps->pic_width_in_mbs_minus1); 
    printf("sps->pic_height_in_map_units_minus1: %d \n", sps->pic_height_in_map_units_minus1); 
    printf("sps->frame_mbs_only_flag: %d \n", sps->frame_mbs_only_flag); 
    if( !sps->frame_mbs_only_flag )
    {
        printf("sps->mb_adaptive_frame_field_flag: %d \n", sps->mb_adaptive_frame_field_flag); 
    }
    printf("sps->direct_8x8_inference_flag: %d \n", sps->direct_8x8_inference_flag); 
    printf("sps->frame_cropping_flag: %d \n", sps->frame_cropping_flag); 
    if( sps->frame_cropping_flag )
    {
        printf("sps->frame_crop_left_offset: %d \n", sps->frame_crop_left_offset); 
        printf("sps->frame_crop_right_offset: %d \n", sps->frame_crop_right_offset); 
        printf("sps->frame_crop_top_offset: %d \n", sps->frame_crop_top_offset); 
        printf("sps->frame_crop_bottom_offset: %d \n", sps->frame_crop_bottom_offset); 
    }
    printf("sps->vui_parameters_present_flag: %d \n", sps->vui_parameters_present_flag);
}

void debug_pps(pps_t* pps)
{
    printf("pps->pic_parameter_set_id: %d \n", pps->pic_parameter_set_id); 
    printf("pps->seq_parameter_set_id: %d \n", pps->seq_parameter_set_id); 
    printf("pps->entropy_coding_mode_flag: %d \n", pps->entropy_coding_mode_flag); 
    printf("pps->pic_order_present_flag: %d \n", pps->pic_order_present_flag); 
    printf("pps->num_slice_groups_minus1: %d \n", pps->num_slice_groups_minus1); 

    if( pps->num_slice_groups_minus1 > 0 )
    {
        printf("pps->slice_group_map_type: %d \n", pps->slice_group_map_type); 
        if( pps->slice_group_map_type == 0 )
        {
            for( int i_group = 0; i_group <= pps->num_slice_groups_minus1; i_group++ )
            {
                printf("pps->run_length_minus1[ i_group ]: %d \n", pps->run_length_minus1[ i_group ]); 
            }
        }
        else if( pps->slice_group_map_type == 2 )
        {
            for( int i_group = 0; i_group < pps->num_slice_groups_minus1; i_group++ )
            {
                printf("pps->top_left[ i_group ]: %d \n", pps->top_left[ i_group ]); 
                printf("pps->bottom_right[ i_group ]: %d \n", pps->bottom_right[ i_group ]); 
            }
        }
        else if( pps->slice_group_map_type == 3 ||
                 pps->slice_group_map_type == 4 ||
                 pps->slice_group_map_type == 5 )
        {
            printf("pps->slice_group_change_direction_flag: %d \n", pps->slice_group_change_direction_flag); 
            printf("pps->slice_group_change_rate_minus1: %d \n", pps->slice_group_change_rate_minus1); 
        }
        else if( pps->slice_group_map_type == 6 )
        {
            printf("pps->pic_size_in_map_units_minus1: %d \n", pps->pic_size_in_map_units_minus1); 
            for( int i = 0; i <= pps->pic_size_in_map_units_minus1; i++ )
            {
                printf("pps->slice_group_id[ i ]: %d \n", pps->slice_group_id[ i ]); 
            }
        }
    }
    printf("pps->num_ref_idx_l0_active_minus1: %d \n", pps->num_ref_idx_l0_active_minus1); 
    printf("pps->num_ref_idx_l1_active_minus1: %d \n", pps->num_ref_idx_l1_active_minus1); 
    printf("pps->weighted_pred_flag: %d \n", pps->weighted_pred_flag); 
    printf("pps->weighted_bipred_idc: %d \n", pps->weighted_bipred_idc); 
    printf("pps->pic_init_qp_minus26: %d \n", pps->pic_init_qp_minus26); 
    printf("pps->pic_init_qs_minus26: %d \n", pps->pic_init_qs_minus26); 
    printf("pps->chroma_qp_index_offset: %d \n", pps->chroma_qp_index_offset); 
    printf("pps->deblocking_filter_control_present_flag: %d \n", pps->deblocking_filter_control_present_flag); 
    printf("pps->constrained_intra_pred_flag: %d \n", pps->constrained_intra_pred_flag); 
    printf("pps->redundant_pic_cnt_present_flag: %d \n", pps->redundant_pic_cnt_present_flag); 

    int have_more_data = 0;
    if( 1 )
    {
        have_more_data = pps->transform_8x8_mode_flag | pps->pic_scaling_matrix_present_flag | pps->second_chroma_qp_index_offset != 0;
    }

    if( have_more_data )
    {
        printf("pps->transform_8x8_mode_flag: %d \n", pps->transform_8x8_mode_flag); 
        printf("pps->pic_scaling_matrix_present_flag: %d \n", pps->pic_scaling_matrix_present_flag); 
        if( pps->pic_scaling_matrix_present_flag )
        {
            for( int i = 0; i < 6 + 2* pps->transform_8x8_mode_flag; i++ )
            {
                printf("pps->pic_scaling_list_present_flag[ i ]: %d \n", pps->pic_scaling_list_present_flag[ i ]);
            }
        }
        printf("pps->second_chroma_qp_index_offset: %d \n", pps->second_chroma_qp_index_offset); 
    }
}

