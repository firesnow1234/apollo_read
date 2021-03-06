/******************************************************************************
 * Copyright 2017 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "modules/common/apollo_app.h"

#include <csignal>
#include <memory>
#include <string>
#include <vector>

#include "gflags/gflags.h"
#include "modules/common/log.h"
#include "modules/common/status/status.h"
#include "modules/common/util/string_util.h"

#include "ros/include/ros/ros.h"

DECLARE_string(log_dir);

namespace apollo {
namespace common {

void ApolloApp::SetCallbackThreadNumber(uint32_t callback_thread_num) {
  CHECK_GE(callback_thread_num, 1);
  callback_thread_num_ = callback_thread_num;
}

void ApolloApp::ExportFlags() const {
  const auto export_file = util::StrCat(FLAGS_log_dir, "/", Name(), ".flags");  
  //    字符合成using google::protobuf::StrCat; 合成结果为输出文件的地址 *.flag
  //    This merges the given strings or numbers, with no delimiter.  This
  //    is designed to be the fastest possible way to construct a string out
  //    of a mix of raw C strings, strings, bool values,
  //    and numeric values.
  //    StrCat supports appending up to 9 arguments
  std::ofstream fout(export_file);      //通过fiostream.h中的ofstream写入flags参数到*.flag文件中
  CHECK(fout) << "Cannot open file " << export_file;  //判断log文件是否可以打开，功能类似fout.is_open()

  std::vector<gflags::CommandLineFlagInfo> flags;     //初始化一个所有flags的数组flags
  gflags::GetAllFlags(&flags);                        //获取所有flags的数据到数组flags中
  for (const auto& flag : flags) {                    //循环写入格式为：
                                                      /****
                                                            # 类型，default=***
                                                            # 描述
                                                            -- 名字=当前值” 
                                                      ****/
                                                      //的log到fout文件中，通过ofstream 的流“ << ”来写入

    fout << "# " << flag.type << ", default=" << flag.default_value << "\n"
         << "# " << flag.description << "\n"
         << "--" << flag.name << "=" << flag.current_value << "\n"
         << std::endl;
  }
}

int ApolloApp::Spin() {
  auto status = Init();//virtual apollo::common::Status Init() = 0; 纯虚函数由子类实现->运行Status Control::Init -> return Status::OK()
  if (!status.ok())    //Status初始化列表 --> Status() : code_(ErrorCode::OK), msg_() {}
  {
    AERROR << Name() << " Init failed: " << status;
    return -1;
  }

  //创建一个空的智能指针，只要unique_ptr指针创建成功，其析构函数都会被调用。确保动态资源被释放。
  //函数结束后,自动释放资源,防止接下来的代码由于抛出异常或者提前退出,而没有执行delete操作

  std::unique_ptr<ros::AsyncSpinner> spinner;
  if (callback_thread_num_ > 1) {
    spinner = std::unique_ptr<ros::AsyncSpinner>(new ros::AsyncSpinner(callback_thread_num_));
            //创建线程std::unique_ptr<T>(new T(int number)); 
            //AsyncSpinner (uint32_t thread_count, CallbackQueue *queue)
  }

  status = Start(); //virtual apollo::common::Status Start() = 0; 纯虚函数由子类实现->运行Status Control::Start() -> return Status::OK();
  if (!status.ok()) {
    AERROR << Name() << " Start failed: " << status;
    return -2;
  }
  ExportFlags(); //调用void ApolloApp::ExportFlags
  if (spinner) {
    spinner->start(); //消息线程开启ros::AsyncSpinner->start()
  } else {
    ros::spin();
  }
  ros::waitForShutdown();//等待触发Shutdown -> ros/init.cpp
  Stop();  //virtual void Stop() = 0; 纯虚函数由子类实现->退出(由子类Control::Stop() 具体重写的)
  AINFO << Name() << " exited.";
  return 0;
}

void apollo_app_sigint_handler(int signal_num) {
  AINFO << "Received signal: " << signal_num;
  if (signal_num != SIGINT) {
    return;
  }
  bool static is_stopping = false;
  if (is_stopping) {
    return;
  }
  is_stopping = true;
  ros::shutdown();
}

}  // namespace common
}  // namespace apollo
