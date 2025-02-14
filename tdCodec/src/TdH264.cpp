#include "TdH264.h"
#include<cmath>

int32_t TdH264::decoderSetUp()
{
    long rv = WelsCreateDecoder (&decoder_);
    if (decoder_ == NULL) 
    {
        return (int32_t)rv;
    }
    memset (&decParam, 0, sizeof (SDecodingParam));
    decParam.uiTargetDqLayer = UCHAR_MAX;
    decParam.eEcActiveIdc = ERROR_CON_SLICE_COPY;
    //decParam.eEcActiveIdc = ERROR_CON_FRAME_COPY_CROSS_IDR;
    decParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
    rv = decoder_->Initialize (&decParam);
    return (int32_t)rv;
}

int32_t TdH264::encoderSetUp(){
    int rv = WelsCreateSVCEncoder (&encoder_);
    if(rv){
        return -1 ;
    }
    unsigned int uiTraceLevel = WELS_LOG_ERROR;
    encoder_->SetOption (ENCODER_OPTION_TRACE_LEVEL, &uiTraceLevel);
    timestamp_ = 0;

    //mTdX264Handle.encoderSetUp();

    return 0 ;
}
//pDecoder->SetOption (DECODER_OPTION_END_OF_STREAM, (void*)&iEndOfStreamFlag);
//pDecoder->FlushFrame (pData, &sDstBufInfo);
int32_t TdH264::setEncoderParam(int width,int height,int bitrate,int iMaxBitrate,int fps)
{
    memset(&encParam,0,sizeof(encParam));
    encoder_->GetDefaultParams(&encParam);
    encParam.iUsageType = CAMERA_VIDEO_REAL_TIME;
    encParam.fMaxFrameRate = fps;
    encParam.iPicWidth = width;
    encParam.iPicHeight = height;
    encParam.iTargetBitrate = bitrate;
    encParam.iRCMode = RC_QUALITY_MODE;
    encParam.iTemporalLayerNum = 1;
    encParam.iSpatialLayerNum = 1;
    encParam.bEnableDenoise = false;
    encParam.bEnableBackgroundDetection = false;
    encParam.bEnableAdaptiveQuant = false;
    encParam.bEnableFrameSkip = false;
    encParam.bEnableLongTermReference = false;
    encParam.uiIntraPeriod = fps;
    encParam.eSpsPpsIdStrategy = CONSTANT_ID;
    encParam.bPrefixNalAddingCtrl = false;
    encParam.sSpatialLayers[0].iVideoWidth = width;
    encParam.sSpatialLayers[0].iVideoHeight = height;
    encParam.sSpatialLayers[0].fFrameRate = fps;

    // for (int i = 0; i < encParam.iSpatialLayerNum; i++) {
    //     encParam.sSpatialLayers[i].iVideoWidth = width ;
    //     encParam.sSpatialLayers[i].iVideoHeight = height ;
    //     encParam.sSpatialLayers[i].fFrameRate = fps;
    // }


    //encParam.sSpatialLayers[0].iSpatialBitrate = bitrate;
    //encParam.sSpatialLayers[0].iMaxSpatialBitrate =5000000;
    encoder_->InitializeExt (&encParam);

    //mTdX264Handle.setEncoderParam(width,height,bitrate,iMaxBitrate,fps);

    return 0 ;
}
void TdH264::setPicture(int width,int height,int Ystride,int UVstride)
{
    memset(&picture_, 0, sizeof(SSourcePicture));
    picture_.iPicHeight = height ;
    picture_.iPicWidth = width ;
    picture_.iColorFormat = videoFormatI420;
    picture_.iStride[0] = Ystride;
    picture_.iStride[1] = UVstride;
    //printf("%d %d\r\n",picture_.iStride[0],picture_.iStride[1]);
}
int32_t TdH264::encode(uint8_t* Ydata,uint8_t* Udata,uint8_t* Vdata,uint8_t** pkt,uint32_t* pkt_size,bool& is_keyframe,bool& got_output){
    got_output = false;
	if (!encoder_)
	{
		return false;
	}
    *pkt =nullptr;
    *pkt_size = 0 ;
	picture_.uiTimeStamp =timestamp_++; 
    picture_.pData[0] = (unsigned char*)Ydata;
    picture_.pData[1] = (unsigned char*)Udata;
    picture_.pData[2] = (unsigned char*)Vdata;

    //uint32_t temporal_id = 0;

	SFrameBSInfo encoded_frame_info;
	int err = encoder_->EncodeFrame(&picture_, &encoded_frame_info);
	if (err) {
        printf("enCodec error\r\n");
		return 0;
	}

	if (encoded_frame_info.eFrameType == videoFrameTypeInvalid) {
        printf("enCodec videoFrameTypeInvalid\r\n");
		return 0;
	}

	if (encoded_frame_info.eFrameType != videoFrameTypeSkip) {
		int iLayer = 0;
        
		while (iLayer < encoded_frame_info.iLayerNum) {
			SLayerBSInfo* pLayerBsInfo = &(encoded_frame_info.sLayerInfo[iLayer]);
			if (pLayerBsInfo != NULL) {
				int iLayerSize = 0;
				//temporal_id = pLayerBsInfo->uiTemporalId;
				int iNalIdx = pLayerBsInfo->iNalCount - 1;
				do {
					iLayerSize += pLayerBsInfo->pNalLengthInByte[iNalIdx];
					--iNalIdx;
				} while (iNalIdx >= 0);
                *pkt = (uint8_t*)realloc(*pkt,*pkt_size+iLayerSize);
				memcpy(*pkt + *pkt_size, pLayerBsInfo->pBsBuf, iLayerSize);
				*pkt_size += iLayerSize;
			}
			++iLayer;
		}
		got_output = true;
	}
	else {
		printf("!!!!videoFrameTypeSkip---!\n");
		is_keyframe = false;
	}
    //printf("FrameType: %d FrameSizeInBytes: %d LayerNum: %d\r\n",encoded_frame_info.eFrameType,encoded_frame_info.iFrameSizeInBytes,encoded_frame_info.iLayerNum);
	if (*pkt_size > 0)
	{
		EVideoFrameType ft_temp = encoded_frame_info.eFrameType;
		if (ft_temp == 1 || ft_temp == 2)
		{
			is_keyframe = true;
		}
		else if (ft_temp == 3)
		{
			is_keyframe = false;
		}
		else
		{
			is_keyframe = false;
		}
	}

	return *pkt_size ;
}

int32_t TdH264::convert(uint8_t *inDate,uint32_t inSize,uint8_t **outDate,uint32_t *outSize,bool &isKeyFrame)
{
    if((inDate==nullptr))
    {
        return -1;
    }
    uint8_t *oData[3];
    SBufferInfo obufInfo ;
    int status;
    int64_t dst_y_plane_size,dst_uv_plane_size;
    int64_t src_y_plane_size,src_uv_plane_size;

    YUVFrame_t srcFrame,*dstFrame;
    memset (&obufInfo, 0, sizeof (SBufferInfo));
    *outSize = 0 ;
    isKeyFrame=false ;
    DECODING_STATE rv = decoder_->DecodeFrame2 (inDate, (int) inSize, oData, &obufInfo);
    if (obufInfo.iBufferStatus == 1)
    {  
        if(obufInfo.UsrData.sSystemBuffer.iFormat==videoFormatI420){            
            srcFrame.data[0].data = oData[0];
            srcFrame.data[1].data = oData[1];
            srcFrame.data[2].data = oData[2];
            srcFrame.height = obufInfo.UsrData.sSystemBuffer.iHeight;
            srcFrame.width = obufInfo.UsrData.sSystemBuffer.iWidth;
            srcFrame.stride[0] = obufInfo.UsrData.sSystemBuffer.iStride[0];
            srcFrame.stride[1] = obufInfo.UsrData.sSystemBuffer.iStride[1];

            dstFrame = newEmptyYUVFrame();
            dstFrame->height = getCodecHeight();
            dstFrame->width = getCodecWidth();

             if(isCodecParamRefresh()){
                dstFrame->height = getCodecHeight();
                dstFrame->width = getCodecWidth();
                //printf("H264修改参数 %d %d \r\n",dstFrame->width,dstFrame->height);
                setEncoderParam(dstFrame->width,dstFrame->height,400000,getCodecMaxBitrate(),getCodecRate());
                // bool enID = true;
                // encoder_->ForceIntraFrame (enID);
                setCodecParamRefresh(false);
            }
            status=scale(&srcFrame,dstFrame);
            if(status)
            {
                return -1;
            }
            bool is_keyframe=false ,got_output=false;         
            setPicture(dstFrame->width,dstFrame->height,dstFrame->stride[0],dstFrame->stride[1]);
            encode(dstFrame->data[0].data,dstFrame->data[1].data,dstFrame->data[2].data,outDate,outSize,isKeyFrame,got_output);
            freeYUVFrame(dstFrame);
        }
    }
    return 0 ;
}

int32_t TdH264::create()
{  
    decoderSetUp();
    encoderSetUp();
    setCodecParamRefresh(true);
    //printf("TdH264::create\r\n");
    return 0 ;
}


int32_t TdH264::destory()
{
    if(encoder_!=nullptr){
        encoder_->Uninitialize();
        WelsDestroySVCEncoder(encoder_);
        encoder_ = nullptr;
        printf("encoder_ destory \r\n");
    }
    printf("encoder_ destory11111 \r\n");
    if(decoder_!=nullptr)
    {
        decoder_->Uninitialize();
        WelsDestroyDecoder(decoder_);
        decoder_ = nullptr;
        printf("decoder_ destory \r\n");
    }
    return 0 ;
}