//  Copyright (c) 2017-present The pika_hub Authors.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.

#include <string>

#include "src/pika_hub_server.h"
#include "slash/include/slash_string.h"

Options SanitizeOptions(const Options& options) {
  Options result(options);
  if (result.info_log == nullptr) {
    rocksutil::Status s = rocksutil::CreateLogger(result.info_log_path,
        &result.info_log, result.env, result.max_log_file_size,
        result.log_file_time_to_roll, result.info_log_level);
    if (!s.ok()) {
      result.info_log = nullptr;
    }
  }
  return result;
}

floyd::Options BuildFloydOptions(const Options& options) {
  floyd::Options result(options.str_members,
      options.local_ip, options.local_port,
      options.data_path, options.log_path);
  result.members = options.members;
  return result;
}

void PikaHubServerHandler::CronHandle() const {
  pika_hub_server_->ResetLastSecQueryNum();
  char buf[1024*1024];
  memset(buf, 'a', 1024*1024);
  pika_hub_server_->binlog_writer()->Append(std::string(buf));
}

bool PikaHubInnerServerHandler::AccessHandle(std::string& ip) const {
  return pika_hub_server_->IsValidClient(ip);
}

PikaHubServer::PikaHubServer(const Options& options)
  : env_(options.env),
    options_(SanitizeOptions(options)),
    statistic_data_(options.env) {
  floyd::Floyd::Open(BuildFloydOptions(options), &floyd_);
  conn_factory_ = new PikaHubClientConnFactory();
  server_handler_ = new PikaHubServerHandler(this);
  server_thread_ = pink::NewHolyThread(options_.port, conn_factory_, 1000,
                            server_handler_);
  inner_server_handler_ = new PikaHubInnerServerHandler(this);
  inner_server_thread_ = pink::NewDispatchThread(options_.port+1000, 2,
                  conn_factory_, 1000, 1000, inner_server_handler_);
  binlog_manager_ = CreateBinlogManager(options.info_log_path, options.env);
  binlog_writer_ = binlog_manager_->AddWriter();
  binlog_sender_ = nullptr;
}

PikaHubServer::~PikaHubServer() {
  server_thread_->StopThread();
  inner_server_thread_->StopThread();
  delete binlog_sender_;
  delete binlog_writer_;
  delete binlog_manager_;
  delete server_thread_;
  delete inner_server_thread_;
  delete conn_factory_;
  delete server_handler_;
  delete floyd_;
}

slash::Status PikaHubServer::Start() {
  if (!CheckPikaServers()) {
    rocksutil::Fatal(options_.info_log, "Invalid pika-servers");
    return slash::Status::Corruption("Invalid pika-server");
  }

  slash::Status result = floyd_->Start();
  if (!result.ok()) {
    rocksutil::Fatal(options_.info_log, "Floyd start failed: %s",
        result.ToString().c_str());
    return result;
  }

  int ret = server_thread_->StartThread();
  if (ret != 0) {
    return slash::Status::Corruption("Start server error");
  }
  ret = inner_server_thread_->StartThread();
  if (ret != 0) {
    return slash::Status::Corruption("Start inner_server error");
  }

  rocksutil::Info(options_.info_log, "Started");
  sleep(1);
  BinlogReader* reader = binlog_manager_->AddReader(1, 0);
  if (reader != nullptr) {
    binlog_sender_ = new BinlogSender("127.0.0.1", 9221, options_.info_log,
                          reader);
    binlog_sender_->StartThread();
  } else {
    rocksutil::Info(options_.info_log, "Create Reader Failed");
  }
  server_mutex_.Lock();
  server_mutex_.Lock();
  server_mutex_.Unlock();
  return slash::Status::OK();
}

bool PikaHubServer::IsValidClient(const std::string& ip) {
  std::string _ip;
  int port = 0;
  for (auto iter = pika_servers_.begin(); iter != pika_servers_.end(); iter++) {
    slash::ParseIpPortString(iter->first, _ip, port);
    if (_ip == ip) {
      return true;
    }
  }
  return false;
}

bool PikaHubServer::CheckPikaServers() {
  std::string str = options_.pika_servers;
  std::string ip;
  int port = 0;
  PikaStatus status;
  size_t prev_pos = str.find_first_not_of(',', 0);
  size_t pos = str.find(',', prev_pos);

  while (prev_pos != std::string::npos || pos != std::string::npos) {
    std::string token(str.substr(prev_pos, pos - prev_pos));
    if (slash::ParseIpPortString(token, ip, port)) {
      pika_servers_.insert(std::map<std::string, PikaStatus>::
                        value_type(token, status));
    } else {
      return false;
    }
    prev_pos = str.find_first_not_of(',', pos);
    pos = str.find_first_of(',', prev_pos);
  }
  return true;
}

