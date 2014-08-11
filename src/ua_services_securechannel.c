#include "ua_services.h"
#include "ua_transport_binary_secure.h"

UA_Int32 Service_OpenSecureChannel(SL_Channel channel,
const UA_OpenSecureChannelRequest* request,
UA_OpenSecureChannelResponse* response)
{
	UA_Int32 retval = UA_SUCCESS;
	SL_channelState channelState;
	//channel takes care of opening process
	retval |= SL_Channel_processOpenRequest(channel, request,response);
	retval |= SL_Channel_getState(channel, &channelState);
	return retval;
}

UA_Int32 Service_CloseSecureChannel(SL_Channel channel, const UA_CloseSecureChannelRequest *request,
UA_CloseSecureChannelResponse *response)
{
	UA_Int32 retval = UA_SUCCESS;
	UA_UInt32 channelId;
	SL_Channel_getChannelId(channel,&channelId);
	SL_ChannelManager_removeChannel(channelId);
// 62451 Part 6 Chapter 7.1.4 - The server does not send a CloseSecureChannel response

return retval;
}
