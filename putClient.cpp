#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <grpc++/grpc++.h>

#include "rpc.grpc.pb.h"

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;

using etcdserverpb::KV;
using etcdserverpb::PutRequest;
using etcdserverpb::PutResponse;

struct AsyncPutCall
{
    AsyncPutCall() {}
    Status status;
    ClientContext context;
    PutResponse response;
    std::unique_ptr<ClientAsyncResponseReader<PutResponse>> response_reader;
};


class AsyncPutClient {

 public:
  explicit AsyncPutClient(std::shared_ptr<Channel> channel)
      : stub_(KV::NewStub(channel)) {
    grpc_thread_.reset(
        new std::thread(std::bind(&AsyncPutClient::GrpcThread, this)));
  }

  bool AsyncCallPut(const std::string& user) {
    if (user == "quit") {
      return false;
    }

    // Data we are sending to the server.
    std::unique_ptr<PutRequest> request = std::make_unique<PutRequest>();
    request->set_key(user);
    request->set_value("value:" + user);
    request->set_prev_kv(true);

    std::cout << " ** Sending request: " << user << std::endl;

    AsyncPutCall* call = new AsyncPutCall();
    call->response_reader =
        stub_->PrepareAsyncPut(&call->context, *request, &cq_);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->response, &call->status, (void*)call);
    return true;
  }

  ~AsyncPutClient() {
    std::cout << "Shutting down client...." << std::endl;
    grpc::Status status;
    cq_.Shutdown();
    grpc_thread_->join();
  }

 private:
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
        AsyncPutCall* call = static_cast<AsyncPutCall*>(got_tag);
        std::cout << "Etcd received: " << call->response.DebugString() << call->response.prev_kv().key() << std::endl;
      }
    }
  }

  ClientContext context_;

  CompletionQueue cq_;

  std::unique_ptr<KV::Stub> stub_;

  PutResponse response_;

  std::unique_ptr<std::thread> grpc_thread_;

  grpc::Status finish_status_ = grpc::Status::OK;
};

int main(int argc, char** argv) {
  AsyncPutClient puter(grpc::CreateChannel(
      "localhost:2379", grpc::InsecureChannelCredentials()));

  std::string key;
  while (true) {
    std::cout << "Enter key (type quit to end): ";
    std::cin >> key;

    if (!puter.AsyncCallPut(key)) {
      std::cout << "Quitting." << std::endl;
      break;
    }
  }
  return 0;
}
