#include <libnl3/netlink/genl/genl.h>
#include <libnl3/netlink/genl/ctrl.h>
#include <libnl3/netlink/netlink.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstring>

using json = nlohmann::json;

#define FAMILY_NAME "ndm_calc_fam"
enum { ADD_CMD = 1, SUB_CMD = 2, MUL_CMD = 3 };
enum { ATTR_MSG = 1 };

static int family_id;

int parse_and_process(const std::string &msg) {
    auto j = json::parse(msg);
    std::string action = j["action"];
    int a = j["arg1"];
    int b = j["arg2"];

    if (action == "add") return a + b;
    if (action == "sub") return a - b;
    if (action == "mul") return a * b;

    throw std::runtime_error("Unknown action");
}

static int msg_handler(struct nl_msg *msg, void *arg) {
    struct nlmsghdr *nlh = nlmsg_hdr(msg);
    struct genlmsghdr *genlh = static_cast<genlmsghdr*>(nlmsg_data(nlh));
    struct nlattr *attrs[2];

    genlmsg_parse(nlh, 0, attrs, 1, nullptr);

    if (attrs[ATTR_MSG]) {
        std::string recv_msg(static_cast<char*>(nla_data(attrs[ATTR_MSG])), nla_len(attrs[ATTR_MSG]));

        int result = parse_and_process(recv_msg);
        json response = { {"result", result} };

        struct nl_sock *sock = static_cast<nl_sock*>(arg);
        struct nl_msg *reply = nlmsg_alloc();

        genlmsg_put(reply, NL_AUTO_PORT, NL_AUTO_SEQ, family_id, 0, 0, genlh->cmd, 1);
        nla_put_string(reply, ATTR_MSG, response.dump().c_str());
        nl_send_auto(sock, reply);
        nlmsg_free(reply);
    }
    std::cout << "Server handled message" << std::endl;
    return NL_OK;
}

int main() {
    struct nl_sock *sock = nl_socket_alloc();
    genl_connect(sock);
    family_id = genl_ctrl_resolve(sock, FAMILY_NAME);

    nl_socket_modify_cb(sock, NL_CB_VALID, NL_CB_CUSTOM, msg_handler, sock);
    nl_socket_add_membership(sock, family_id);

    std::cout << "Server listening..." << std::endl;
    while (true) nl_recvmsgs_default(sock);

    nl_socket_free(sock);
    return 0;
}
