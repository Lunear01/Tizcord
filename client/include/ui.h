#ifndef UI_H
#define UI_H

#include "../../shared/protocol.h"

void start_ui(void);
void ui_handle_auth_response(TizcordPacket *packet);
void ui_receive_channel_message(TizcordPacket *packet);
void ui_receive_dm_message(TizcordPacket *packet);
void ui_update_server_state(TizcordPacket *packet);
void ui_force_redraw(void);

#endif
