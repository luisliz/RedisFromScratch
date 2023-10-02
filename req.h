#include "common.h"
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <string>
#include <cstring>

enum {
    RES_OK = 0,
    RES_ERR = 1,
    RES_NX = 2,
};

static std::map<std::string, std::string> map; 

static bool cmd_is(const std::string &word, const char *cmd) {
    return 0 == strcasecmp(word.c_str(), cmd);
}

static int32_t parse_req(const uint8_t *data, size_t req_len, std::vector<std::string> &out) {
    if (req_len < 4) {
        return -1;
    }

    uint32_t n = 0;
    memcpy(&n, &data[0], 4);
    if (n > k_max) {
        return -1;
    }

    size_t pos = 4;
    while (n--) {
        if (pos +4 > req_len) {
            return -1;
        }

        uint32_t sz = 0;
        memcpy(&sz, &data[pos], 4); 
        if (pos + 4 + sz > req_len) {
            return -1;
        }

        out.push_back(std::string((char *)&data[pos + 4], sz));
        pos += 4 + sz;
    }

    if (pos != req_len) {
        return -1;
    }
    return 0;
}


static int32_t do_get(const std::vector<std::string> &cmd, uint8_t *res, uint32_t *res_len) {
    if (!map.count(cmd[1])) {
        return RES_NX;
    }

    std::string &val = map[cmd[1]];
    assert(val.size() <= k_max);
    memcpy(res, val.data(), val.size());
    *res_len = (uint32_t)val.size();
    return RES_OK;
}

static int32_t do_set(
    const std::vector<std::string> &cmd, uint8_t *res, uint32_t *res_len) {
    (void)res;
    (void)res_len;

    map[cmd[1]] = cmd[2];
    return RES_OK;
}

static int32_t do_del(
    const std::vector<std::string> &cmd, uint8_t *res, uint32_t *res_len) {
    (void)res;
    (void)res_len;

    if (!map.count(cmd[1])) {
        return RES_NX;
    }

    map.erase(cmd[1]);
    return RES_OK;
}

static int32_t print_map(
    const std::vector<std::string> &cmd, uint8_t *res, uint32_t *res_len) {
    (void)cmd;
    (void)res;
    (void)res_len;

    for (const auto &kv : map) {
        fprintf(stderr, "%s -> %s\n", kv.first.c_str(), kv.second.c_str());
    }
    return RES_OK;
}

static int32_t do_request(
    const uint8_t *req, uint32_t req_len,
    uint32_t *rescode, uint8_t *res, uint32_t *res_len) {
    std::vector<std::string> cmd; 
    if (0 != parse_req(req, req_len, cmd)) {
        msg("bad req");
        return -1;
    }

    if(cmd.size() == 2 && cmd_is(cmd[0], "get")) {
       *rescode = do_get(cmd, res, res_len); 
       msg("get");
    } else if (cmd.size() == 3 && cmd_is(cmd[0], "set")) {
        *rescode = do_set(cmd, res, res_len);
        msg("set");
    } else if (cmd.size() == 2 && cmd_is(cmd[0], "del")) {
        *rescode = do_del(cmd, res, res_len);
        msg("del");
    } else if (cmd.size() == 1 && cmd_is(cmd[0], "print")) {
        *rescode = print_map(cmd, res, res_len);
    } else {
        *rescode = RES_ERR; 
        const char *msg = "Unknown command";
        strcpy((char *)res, msg);
        *res_len = (uint32_t)strlen(msg);
        return 0;
    }

    return 0;
}