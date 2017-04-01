/*
    Globals.h - This file is part of Element
    -2016  Kushview, LLC.  All rights reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef ELEMENT_GLOBALS_H
#define ELEMENT_GLOBALS_H

#include "element/Juce.h"
#include "engine/AudioEngine.h"
#include "URIs.h"
#include "WorldBase.h"

namespace Element {

class CommandManager;
class DeviceManager;
class MediaManager;
class PluginManager;
class Session;
class Settings;
class Writer;

struct CommandLine {
    explicit CommandLine (const String& cli = String::empty);
    bool fullScreen;
    int port;
};

class Globals :  public WorldBase
{
public:
    explicit Globals (const String& commandLine = String::empty);
    ~Globals();

    const CommandLine cli;

    CommandManager& getCommands();
    DeviceManager& devices();
    PluginManager& plugins();
    Settings& settings();
    SymbolMap& symbols();
    MediaManager& media();
    Session& session();
    AudioEnginePtr engine() const;

    const String& getAppName() const { return appName; }
    void setEngine (EnginePtr engine);

private:
    String appName;
    friend class Application;
    class Impl;
    ScopedPointer<Impl> impl;
};

}

#endif // ELEMENT_GLOBALS_H
