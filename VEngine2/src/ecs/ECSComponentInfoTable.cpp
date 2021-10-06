#include "ECSComponentInfoTable.h"

int64_t ECSComponentInfoTable::s_highestRegisteredComponentID = -1;
ECSComponentInfo ECSComponentInfoTable::s_ecsComponentInfo[k_ecsMaxComponentTypes];