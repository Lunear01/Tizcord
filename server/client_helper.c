#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/socket.h>
#include "client_helper.h"
#include "protocol.h"

static int32_t next_list_id = 1;

static int32_t allocate_list_id() {
	if (next_list_id <= 0) {
		next_list_id = 1;
	}
	return next_list_id++;
}

int send_packet_to_client(int client_fd, const TizcordPacket *packet) {
	if (client_fd < 0 || packet == NULL) {
		fprintf(stderr, "[Client Helper] Invalid arguments to send_packet_to_client\n");
		return -1;
	}

	ssize_t bytes_sent = send(client_fd, packet, sizeof(TizcordPacket), 0);
	if (bytes_sent < 0) {
		fprintf(stderr, "[Client Helper] Failed to send packet: %s\n", strerror(errno));
		return -1;
	} else if (bytes_sent != sizeof(TizcordPacket)) {
		fprintf(stderr, "[Client Helper] Partial packet sent: %zd bytes\n", bytes_sent);
		return -1;
	}
	return 0;
}	

int send_action_response(int client_fd, PacketType type, int action, int status_code,
						 const char *message) {
	TizcordPacket response;
	memset(&response, 0, sizeof(response));
	response.type = type;

	switch (type) {
		case PACKET_AUTH:
			response.payload.auth.action = (AuthAction)action;
			response.payload.auth.status_code = status_code;
			break;
		case PACKET_DM:
			response.payload.dm.action = (DMAction)action;
			response.payload.dm.status_code = status_code;
			break;
		case PACKET_SERVER:
			response.payload.server.action = (ServerAction)action;
			response.payload.server.status_code = status_code;
			break;
		case PACKET_CHANNEL:
			response.payload.channel.action = (ChannelAction)action;
			response.payload.channel.status_code = status_code;
			break;
		case PACKET_SOCIAL:
			response.payload.social.action = (SocialAction)action;
			response.payload.social.status_code = status_code;
			break;
		default:
			response.payload.system.action = (SystemAction)action;
			response.payload.system.status_code = status_code;
			if (message != NULL) {
				strncpy(response.payload.system.message, message, SYSTEM_MESSAGE_LEN - 1);
			}
			break;
	}

	return send_packet_to_client(client_fd, &response);
}

int send_list_item_response(int client_fd, const TizcordPacket *item_packet) {
	if (item_packet == NULL) {
		fprintf(stderr, "[Client Helper] item_packet is NULL in send_list_item_response\n");
		return -1;
	}
	return send_packet_to_client(client_fd, item_packet);
}

int send_list_response(int client_fd, PacketType type, int action,
                   const char *const *items, const int64_t *item_ids,
                   const int *item_counts, size_t item_count) {
	if (item_count > 0 && items == NULL) {
		fprintf(stderr, "[Client Helper] items is NULL in send_list_response\n");
		return -1;
	}

	if (item_count > INT32_MAX) {
		fprintf(stderr, "[Client Helper] item_count too large in send_list_response\n");
		return -1;
	}

	int32_t list_id = allocate_list_id();
	int32_t total = (int32_t)item_count;

	if (item_count == 0) {
		TizcordPacket empty_packet;
		memset(&empty_packet, 0, sizeof(empty_packet));
		empty_packet.type = type;
		empty_packet.list_id = list_id;
		empty_packet.list_index = -1;
		empty_packet.list_total = 0;
		empty_packet.list_frame = LIST_FRAME_SINGLE;

		switch (type) {
			case PACKET_SERVER:
				empty_packet.payload.server.action = (ServerAction)action;
				empty_packet.payload.server.status_code = RESP_DONE;
				break;
			case PACKET_CHANNEL:
				empty_packet.payload.channel.action = (ChannelAction)action;
				empty_packet.payload.channel.status_code = RESP_DONE;
				break;
			case PACKET_SOCIAL:
				empty_packet.payload.social.action = (SocialAction)action;
				empty_packet.payload.social.status_code = RESP_DONE;
				break;
			default:
				fprintf(stderr, "[Client Helper] Unsupported list packet type %d\n", type);
				return -1;
		}

		return send_packet_to_client(client_fd, &empty_packet);
	}

	for (size_t i = 0; i < item_count; i++) {
		TizcordPacket item_packet;
		memset(&item_packet, 0, sizeof(item_packet));
		item_packet.type = type;
		item_packet.list_id = list_id;
		item_packet.list_index = (int32_t)i;
		item_packet.list_total = total;

		if (item_count == 1) {
			item_packet.list_frame = LIST_FRAME_SINGLE;
		} else if (i == 0) {
			item_packet.list_frame = LIST_FRAME_START;
		} else if (i == item_count - 1) {
			item_packet.list_frame = LIST_FRAME_END;
		} else {
			item_packet.list_frame = LIST_FRAME_MIDDLE;
		}

		switch (type) {
			case PACKET_SERVER:
				item_packet.payload.server.action = (ServerAction)action;
				item_packet.payload.server.status_code = RESP_OK;
				if (item_ids != NULL) {
					item_packet.payload.server.server_id = item_ids[i];
				}
				if (item_counts != NULL) {
                    item_packet.payload.server.member_count = item_counts[i];
                }
				strncpy(item_packet.payload.server.server_name, items[i], MAX_NAME_LEN - 1);
				break;
			case PACKET_CHANNEL:
				item_packet.payload.channel.action = (ChannelAction)action;
				item_packet.payload.channel.status_code = RESP_OK;
				if (item_ids != NULL) {
					item_packet.payload.channel.channel_id = item_ids[i];
				}
				strncpy(item_packet.payload.channel.channel_name, items[i], MAX_NAME_LEN - 1);
				break;
			case PACKET_SOCIAL:
				item_packet.payload.social.action = (SocialAction)action;
				item_packet.payload.social.status_code = RESP_OK;
				if (item_ids != NULL) {
					item_packet.payload.social.target_user_id = item_ids[i];
				}
				strncpy(item_packet.payload.social.target_username, items[i], MAX_NAME_LEN - 1);
				break;
			default:
				fprintf(stderr, "[Client Helper] Unsupported list packet type %d\n", type);
				return -1;
		}

		if (send_packet_to_client(client_fd, &item_packet) != 0) {
			return -1;
		}
	}

	return 0;
}

int send_list_end_response(int client_fd, PacketType type) {
	TizcordPacket end_packet;
	memset(&end_packet, 0, sizeof(end_packet));
	end_packet.type = type;
	end_packet.list_id = 0; // list_id can be 0 for end packet since it's not part of the list stream
	end_packet.list_index = -1;
	end_packet.list_total = -1;
	end_packet.list_frame = LIST_FRAME_END;

	switch (type) {
		case PACKET_SERVER:
			end_packet.payload.server.status_code = RESP_DONE;
			break;
		case PACKET_CHANNEL:
			end_packet.payload.channel.status_code = RESP_DONE;
			break;
		case PACKET_SOCIAL:
			end_packet.payload.social.status_code = RESP_DONE;
			break;
		default:
			fprintf(stderr, "[Client Helper] Unsupported list packet type %d for end response\n", type);
			return -1;
	}

	return send_packet_to_client(client_fd, &end_packet);
}


