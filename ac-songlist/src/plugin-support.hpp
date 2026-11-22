#pragma once

#include <obs-module.h>

#include <QString>

// Texto traducido desde data/locale/*.ini
inline QString T(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}
