#include <stdint.h>
#include <mach/mach.h>
#include <darwin/macho.h>


struct dyld_all_image_infos * my_get_all_image_infos()
{
    kern_return_t kr;
    task_flavor_t flavor = TASK_DYLD_INFO;
    task_dyld_info_data_t infoData;
    mach_msg_type_number_t task_info_outCnt = TASK_DYLD_INFO_COUNT;
    kr = task_info(mach_task_self(), flavor, (task_info_t) &infoData, &task_info_outCnt);
    if (kr != KERN_SUCCESS) {
        //KR_ERROR(kr);
        return 0;
    }
    struct dyld_all_image_infos *allImageInfos =
            (struct dyld_all_image_infos *) infoData.all_image_info_addr;
    return allImageInfos;
}