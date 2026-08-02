#pragma once
#include <includes.h>

class Global
{
public:
	CNet* m_pNet{ nullptr };
};
inline Global* global = new Global();