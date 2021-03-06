//  Copyright (c) 2017-present The pika_hub Authors.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//

#include "src/pika_hub_command.h"

#include <utility>

#include "src/pika_hub_admin.h"
/* Table for CmdInfo */
static std::unordered_map<std::string, CmdInfo*> cmd_infos(100);

void InitCmdInfoTable() {
  CmdInfo* infoptr = new CmdInfo(kCmdNameInfo, -1,
      kCmdFlagsRead | kCmdFlagsAdmin);
  cmd_infos.insert(std::pair<std::string, CmdInfo*>(kCmdNameInfo, infoptr));
}

void DestoryCmdInfoTable() {
  for (auto it = cmd_infos.begin(); it != cmd_infos.end(); ++it) {
    delete it->second;
  }
}

const CmdInfo* GetCmdInfo(const std::string& opt) {
  auto it = cmd_infos.find(opt);
  if (it != cmd_infos.end()) {
    return it->second;
  }
  return nullptr;
}

void InitCmdTable(std::unordered_map<std::string, Cmd*> *cmd_table) {
  Cmd* infoptr = new InfoCmd();
  cmd_table->insert(std::pair<std::string, Cmd*>(kCmdNameInfo, infoptr));
}

Cmd* GetCmdFromTable(const std::string& opt, const CmdTable& cmd_table) {
  auto it = cmd_table.find(opt);
  if (it != cmd_table.end()) {
    return it->second;
  }
  return NULL;
}

void DestoryCmdTable(CmdTable* cmd_table) {
  for (auto it = cmd_table->begin(); it != cmd_table->end(); ++it) {
    delete it->second;
  }
}
