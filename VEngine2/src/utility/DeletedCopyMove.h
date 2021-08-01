#pragma once

#ifndef DELETED_COPY_MOVE
#define DELETED_COPY_MOVE(T)							\
			T(const T &) = delete;						\
			T(T &&) = delete;							\
			T &operator=(const T &) = delete;			\
			T &operator=(T &&) = delete; 
#endif