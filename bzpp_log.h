#ifndef _BZPP_LOG_H_
#define _BZPP_LOG_H_



#define BZPP_DBG_LOG(fmt, ...) printf(fmt"\n", ##__VA_ARGS__)
#define BZPP_INFO_LOG(fmt, ...)printf(fmt"\n", ##__VA_ARGS__)
#define BZPP_ERR_LOG(fmt, ...) printf(fmt"\n", ##__VA_ARGS__)
#define BZPP_WARN_LOG(fmt, ...) printf(fmt"\n", ##__VA_ARGS__)

#define BZPP_ERR(fmt, ...) printf(fmt"\n", ##__VA_ARGS__)
#define BZPP_INFO(fmt, ...) printf(fmt"\n", ##__VA_ARGS__)

#define BZPP_BUG(fmt, ...) printf(fmt"\n", ##__VA_ARGS__)



#endif

