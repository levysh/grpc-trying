#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <grpc++/grpc++.h>

#include "rpc.grpc.pb.h"

using grpc::Channel;
using grpc::ClientAsyncReaderWriter;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;

using etcdserverpb::Watch;
using etcdserverpb::WatchRequest;
using etcdserverpb::WatchResponse;
using etcdserverpb::WatchCreateRequest;
using etcdserverpb::PutRequest;
using etcdserverpb::PutResponse;


class AsyncBidiWatchClient {
  enum class Type {
    READ = 1,
    WRITE = 2,
    CONNECT = 3,
    WRITES_DONE = 4,
    FINISH = 5
  };

 public:
  explicit AsyncBidiWatchClient(std::shared_ptr<Channel> channel)
      : stub_(Watch::NewStub(channel)) {
    grpc_thread_.reset(
        new std::thread(std::bind(&AsyncBidiWatchClient::GrpcThread, this)));
    stream_ = stub_->AsyncWatch(&context_, &cq_,
                                   reinterpret_cast<void*>(Type::CONNECT));
  }

  bool AsyncCallWatch(const std::string& user) {
    if (user == "quit") {
      stream_->WritesDone(reinterpret_cast<void*>(Type::WRITES_DONE));
      return false;
    }

    WatchRequest request;
    WatchCreateRequest create_request;
    create_request.set_key(user);
    request.mutable_create_request()->CopyFrom(create_request);

    std::cout << " ** Sending request: " << user << std::endl;
    stream_->Write(request, reinterpret_cast<void*>(Type::WRITE));
    return true;
  }

  ~AsyncBidiWatchClient() {
    std::cout << "Shutting down client...." << std::endl;
    grpc::Status status;
    cq_.Shutdown();
    grpc_thread_->join();
  }

 private:
  void AsyncWatchRequestNextMessage() {
    std::cout << " ** Got response: " << response_.created() << response_.DebugString() << std::endl;

    stream_->Read(&response_, reinterpret_cast<void*>(Type::READ));
  }

  void GrpcThread() {
    while (true) {
      void* got_tag;
      bool ok = false;

      if (!cq_.Next(&got_tag, &ok)) {
        std::cerr << "Client stream closed. Quitting" << std::endl;
        break;
      }

      if (ok) {
        std::cout << std::endl
                  << "**** Processing completion queue tag " << got_tag
                  << std::endl;
        switch (static_cast<Type>(reinterpret_cast<long>(got_tag))) {
          case Type::READ:
            std::cout << "Read a new message." << std::endl;
            break;
          case Type::WRITE:
            std::cout << "Sending message (async)." << std::endl;
            AsyncWatchRequestNextMessage();
            break;
          case Type::CONNECT:
            std::cout << "Server connected." << std::endl;
            break;
          case Type::WRITES_DONE:
            std::cout << "Server disconnecting." << std::endl;
            break;
          case Type::FINISH:
            std::cout << "Client finish; status = "
                      << (finish_status_.ok() ? "ok" : "cancelled")
                      << std::endl;
            context_.TryCancel();
            cq_.Shutdown();
            break;
          default:
            std::cerr << "Unexpected tag " << got_tag << std::endl;
            GPR_ASSERT(false);
        }
      }
    }
  }

  ClientContext context_;

  CompletionQueue cq_;

  std::unique_ptr<Watch::Stub> stub_;

  std::unique_ptr<ClientAsyncReaderWriter<WatchRequest, WatchResponse>> stream_;

  WatchResponse response_;

  std::unique_ptr<std::thread> grpc_thread_;

  grpc::Status finish_status_ = grpc::Status::OK;
};

int main(int argc, char** argv) {
  AsyncBidiWatchClient watcher(grpc::CreateChannel(
      "localhost:2379", grpc::InsecureChannelCredentials()));

  std::string text;
  while (true) {
    std::cout << "Enter text (type quit to end): ";
    std::cin >> text;

    if (!watcher.AsyncCallWatch(text)) {
      std::cout << "Quitting." << std::endl;
      break;
    }
  }
  return 0;
}
