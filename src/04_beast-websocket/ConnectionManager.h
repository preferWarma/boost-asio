#ifndef CONNECTION_MANAGER_H_
#define CONNECTION_MANAGER_H_

#include "Connection.h"
#include "lyf.h"
#include <unordered_map>

using std::shared_ptr;
using std::string;
using std::unordered_map;

class ConnectionManager : public lyf::Singleton<ConnectionManager> {
    friend class lyf::Singleton<ConnectionManager>;

public:
    void
    AddConnection(shared_ptr<Connection> connection) {
        _connections[connection->Uid()] = connection;
    }

    void
    RemoveConnection(shared_ptr<Connection> connection) {
        _connections.erase(connection->Uid());
    }

private:
    ConnectionManager() {}

private:
    unordered_map<string, shared_ptr<Connection>> _connections;
};

#endif // CONNECTION_MANAGER_H_
