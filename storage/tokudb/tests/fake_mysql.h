// Provide some mimimal MySQL classes just to compile the tokudb cardinality functions
class KEY_INFO {
public:
    uint flags;
    uint key_parts;
    uint64_t *rec_per_key;
    char *name;
};
#define HA_NOSAME 1
class TABLE_SHARE {
public:
    uint primary_key;
    uint keys, key_parts;
    KEY_INFO *key_info;
};
class TABLE {
public:
    TABLE_SHARE *s;
    KEY_INFO *key_info;
};
uint get_key_parts(KEY_INFO *key_info) {
    assert(key_info);
    return 0;
}
