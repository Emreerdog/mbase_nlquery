#ifndef MBASE_NLQ_GLOBAL_DEF_H
#define MBASE_NLQ_GLOBAL_DEF_H

#include <mbase/synchronization.h>
#include "model_proc_cl.h"

inline mbase::I32 gMaxRows = 1000;
inline mbase::I32 gUserCount = 2;
inline mbase::I32 gListenPort = 8080;
inline bool gIsWebui = true;
inline mbase::NlqModel* gGlobalModel;
inline mbase::mutex gLoopSync;
inline mbase::string gListenHostname = "127.0.0.1";
inline mbase::string gSSLPublicPath;
inline mbase::string gSSLPrivatePath;

#endif //