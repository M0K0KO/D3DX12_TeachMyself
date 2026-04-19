#pragma once
#include "Entity.h"

struct HierarchyComponent
{
	Entity parent = INVALID_ENTITY;
	Entity firstChild = INVALID_ENTITY;
	Entity nextSibling = INVALID_ENTITY;
	Entity prevSibling = INVALID_ENTITY;
};