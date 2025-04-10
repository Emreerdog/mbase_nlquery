#ifndef MBASE_NLQ_GLOBAL_DEF_H
#define MBASE_NLQ_GLOBAL_DEF_H

#include <mbase/synchronization.h>
#include "model_proc_cl.h"

inline mbase::I32 gMaxRows = 1000;
inline mbase::NlqModel* gGlobalModel;
inline mbase::mutex gLoopSync;

#endif //