#include <libnl3/netlink/genl/genl.h>
#include <libnl3/netlink/genl/ctrl.h>
#include <libnl3/netlink/netlink.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstring>
#include <libnl3/netlink/msg.h>
#include <libnl3/netlink/attr.h>
#include <libnl3/netlink/socket.h>

using json = nlohmann::json;

#define FAMILY_NAME "ndm_calc_fam"
enum { ADD_CMD = 1, SUB_CMD = 2, MUL_CMD = 3 };
enum { ATTR_MSG = 1 };

static int family_id;

static int response_handler(struct nl_msg *msg, void *arg) {
    std::cout << "responce handler is called!" << std::endl;
    struct nlmsghdr *nlh = nlmsg_hdr(msg);
    struct nlattr *attrs[2];
    int err = genlmsg_parse(nlh, 0, attrs, 1, nullptr);
    if (err < 0) {
        return err;
    }

    if (attrs[ATTR_MSG]) {
        std::string recv_msg(static_cast<char*>(nla_data(attrs[ATTR_MSG])), nla_len(attrs[ATTR_MSG]));
        std::cout << "Response from server: " << recv_msg << std::endl;
    }
    return NL_OK;
}

void send_request(struct nl_sock *sock, const std::string &action, int arg1, int arg2) {
    struct nl_msg *msg = nlmsg_alloc();
    int cmd = (action == "add") ? ADD_CMD : (action == "sub") ? SUB_CMD : MUL_CMD;

    json j = {
        {"action", action},
        {"arg1", arg1},
        {"arg2", arg2}
    };

    genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, family_id, 0, NLM_F_ACK, cmd, 1);
    nla_put_string(msg, ATTR_MSG, j.dump().c_str());

    nl_send_auto_complete(sock, msg);
    nlmsg_free(msg);
}

int main() {
    struct nl_sock *sock = nl_socket_alloc();
    if (!sock) {
        std::cerr << "Failed to allocate netlink socket" << std::endl;
        return 1;
    }

    int err = genl_connect(sock);
    if (err < 0) {
        std::cerr << "Failed to connect generic netlink: " << nl_geterror(err) << std::endl;
        nl_socket_free(sock);
        return 1;
    }

    family_id = genl_ctrl_resolve(sock, FAMILY_NAME);
    if (family_id < 0) {
        std::cerr << "Failed to resolve family id for " << FAMILY_NAME << ": " << nl_geterror(family_id) << std::endl;
        return 1;
    }

    err = nl_socket_modify_cb(sock, NL_CB_VALID, NL_CB_CUSTOM, response_handler, nullptr);
    if (err < 0) {
        std::cerr << "Failed to ivoke callback in responce: " << nl_geterror(err) << std::endl;
        return 1;
    }
    
    // Hard code the message
    send_request(sock, "add", 1, 2);
    err = nl_recvmsgs_default(sock);
    if (err < 0) {
        std::cerr << "Error receiving message: " << nl_geterror(err) << std::endl;
    }

    nl_socket_free(sock);
    return 0;
}
