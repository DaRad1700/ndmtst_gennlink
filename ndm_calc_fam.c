// ndm_calc_fam.c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/genetlink.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include <linux/json.h> 

#define FAMILY_NAME "ndm_calc_fam"

enum {
    ATTR_UNSPEC,
    ATTR_MSG,   // string message (JSON)
    __ATTR_MAX,
};
#define ATTR_MAX (__ATTR_MAX - 1)

enum {
    CMD_UNSPEC,
    ADD_CMD,
    SUB_CMD,
    MUL_CMD,
    __CMD_MAX,
};
#define CMD_MAX (__CMD_MAX - 1)

static struct nla_policy calc_policy[ATTR_MAX + 1] = {
    [ATTR_MSG] = { .type = NLA_NUL_STRING },
};

static struct genl_family ndm_calc_fam;

static int calc_handle_cmd(struct sk_buff *skb, struct genl_info *info) {
    struct nlattr *na;
    char *msg;
    char response[256];
    int arg1 = 0, arg2 = 0, result = 0;
    char action[16] = {0};

    if (!info->attrs[ATTR_MSG]) {
        pr_info("ndm_calc_fam: no ATTR_MSG\n");
        return -EINVAL;
    }

    na = info->attrs[ATTR_MSG];
    msg = nla_data(na);

    // Simple JSON parsing: look for "action", "arg1", "arg2"
    // This is a minimal parser, not robust!

    if (sscanf(msg, "{\"action\":\"%15[^\"]\",\"arg1\":%d,\"arg2\":%d}", action, &arg1, &arg2) != 3) {
        pr_info("ndm_calc_fam: bad JSON format: %s\n", msg);
        return -EINVAL;
    }

    if (strcmp(action, "add") == 0) {
        result = arg1 + arg2;
    } else if (strcmp(action, "sub") == 0) {
        result = arg1 - arg2;
    } else if (strcmp(action, "mul") == 0) {
        result = arg1 * arg2;
    } else {
        pr_info("ndm_calc_fam: unknown action: %s\n", action);
        return -EINVAL;
    }

    snprintf(response, sizeof(response),
        "{\"action\":\"%s\",\"arg1\":%d,\"arg2\":%d,\"result\":%d}",
        action, arg1, arg2, result);

    // Reply to sender
    {
        struct sk_buff *skb_out;
        void *hdr;
        int ret;

        skb_out = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
        if (!skb_out)
            return -ENOMEM;

        hdr = genlmsg_put(skb_out, 0, info->snd_seq, &ndm_calc_fam, 0, info->genlhdr->cmd);
        if (!hdr) {
            nlmsg_free(skb_out);
            return -ENOMEM;
        }

        ret = nla_put_string(skb_out, ATTR_MSG, response);
        if (ret != 0) {
            nlmsg_free(skb_out);
            return ret;
        }

        genlmsg_end(skb_out, hdr);

        ret = genlmsg_reply(skb_out, info);
        if (ret != 0) {
            pr_info("ndm_calc_fam: genlmsg_reply failed: %d\n", ret);
            return ret;
        }
    }

    pr_info("ndm_calc_fam: handled %s %d %d = %d\n", action, arg1, arg2, result);
    return 0;
}

static const struct genl_ops calc_ops[] = {
    {
        .cmd = ADD_CMD,
        .flags = 0,
        .policy = calc_policy,
        .doit = calc_handle_cmd,
        .dumpit = NULL,
    },
    {
        .cmd = SUB_CMD,
        .flags = 0,
        .policy = calc_policy,
        .doit = calc_handle_cmd,
        .dumpit = NULL,
    },
    {
        .cmd = MUL_CMD,
        .flags = 0,
        .policy = calc_policy,
        .doit = calc_handle_cmd,
        .dumpit = NULL,
    },
};

static struct genl_family ndm_calc_fam = {
    .hdrsize = 0,
    .name = FAMILY_NAME,
    .version = 1,
    .maxattr = ATTR_MAX,
    .ops = calc_ops,
    .n_ops = ARRAY_SIZE(calc_ops),
    .module = THIS_MODULE,
};

static int __init calc_init(void) {
    int ret;

    ret = genl_register_family(&ndm_calc_fam);
    if (ret != 0) {
        pr_err("ndm_calc_fam: failed to register family: %d\n", ret);
        return ret;
    }

    pr_info("ndm_calc_fam: registered successfully\n");
    return 0;
}

static void __exit calc_exit(void) {
    genl_unregister_family(&ndm_calc_fam);
    pr_info("ndm_calc_fam: unregistered\n");
}

module_init(calc_init);
module_exit(calc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("mikhail");
MODULE_DESCRIPTION("Module for registering family in kernel");
