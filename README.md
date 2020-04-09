#### Что пытаюсь сделать
Возпользоваться async bidi stream, для Watch запросов Etcd.

#### Где искал информацию
Единственное что смог найти более менее внятное это https://github.com/grpc/grpc/pull/8934, но он так и не влит, так как есть проблемы, о которых написанно в самом пр.

#### Что делал
- писал класс Etcdkeeper, в котором реализовывал функционал в том виде в котором он должен быть (долгий способ, так как долго пересобирать)
- модифицировал client-а из PR (bidiClient.cpp в этом репозитории и putClient.cpp, он нужен для сравнения Bidirectional streaming RPC с Unary RPC)

#### Как запустить 
- установить grpc https://github.com/grpc/grpc
- установить protobuf https://github.com/protocolbuffers/protobuf
- установить Etcd и запустить сервер https://etcd.io/docs/v3.4.0/dl-build/
- скомпилировать протобафы `protoc -I protos/ --cpp_out=. protos/rpc.proto`, `protoc -I protos/ --grpc_out=. --plugin=protoc-gen-grpc=grpc_cpp_plugin protos/rpc.proto`
- собрать ```c++ bidiClient.cpp rpc.grpc.pb.cc  rpc.pb.cc `pkg-config --cflags --libs protobuf grpc++` -o client```
- запустить `./client`, ввести какой нибудь ключ и изменить руками ключ в etcd через клиента

#### Что работает не так как ожидалось
WatchResponse-ы приходят с запазданием, и только как ответ на мой следующий write. Ожидалось что CompletionQueue будет отрабатывать сразу после изменения значения в etcd.
