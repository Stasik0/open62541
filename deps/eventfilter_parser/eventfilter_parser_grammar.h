/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *    Copyright 2023 (c) Fraunhofer IOSB (Author: Florian Düwel)
 */

#ifndef OPEN62541_EVENTFILTER_PARSER_GRAMMAR_H
#define OPEN62541_EVENTFILTER_PARSER_GRAMMAR_H

#include "open62541/plugin/log_stdout.h"
#include "open62541/server.h"
#include "open62541/server_config_default.h"
#include "open62541_queue.h"
#include "eventfilter_parser.h"

UA_StatusCode UA_EventFilter_parse(UA_ByteString *content, UA_EventFilter *filter);
void clear_event_filter(UA_EventFilter *filter);

#endif  // OPEN62541_EVENTFILTER_PARSER_GRAMMAR_H
