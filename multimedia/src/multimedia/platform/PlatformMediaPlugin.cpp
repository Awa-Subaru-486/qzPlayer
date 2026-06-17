// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "PlatformMediaPlugin_p.h"

PlatformMediaPlugin::PlatformMediaPlugin(QObject *parent) : QObject(parent) { }

PlatformMediaPlugin::~PlatformMediaPlugin() = default;

#include "moc_PlatformMediaPlugin_p.cpp"
