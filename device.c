#include "aw5808.h"

void devices_init()
{
    // init by board or config.ini
    switch(board) {
        msk28:
            aw5808_t *aw5808 = aw5808_new();
            aw5808_open(aw5808, &opt)
            aw5808_add_device(aw5808);
            aw5808_t *aw5808 = aw5808_new();
            aw5808_open(aw5808, &opt2)
            aw5808_add_device(aw5808);
    }
}

aw5808_t get_aw5808(int index)
{
    aw5808_t *aw5808
    return aw5808;
}