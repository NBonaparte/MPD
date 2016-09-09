/*
 * Copyright 2003-2016 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "Init.hxx"
#include "Registry.hxx"
#include "InputPlugin.hxx"
#include "util/Error.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigOption.hxx"
#include "config/Block.hxx"
#include "Log.hxx"
#include "PluginUnavailable.hxx"
#include "util/RuntimeError.hxx"

#include <stdexcept>

#include <assert.h>

bool
input_stream_global_init(Error &error)
{
	const ConfigBlock empty;

	for (unsigned i = 0; input_plugins[i] != nullptr; ++i) {
		const InputPlugin *plugin = input_plugins[i];

		assert(plugin->name != nullptr);
		assert(*plugin->name != 0);
		assert(plugin->open != nullptr);

		const auto *block =
			config_find_block(ConfigBlockOption::INPUT, "plugin",
					  plugin->name);
		if (block == nullptr) {
			block = &empty;
		} else if (!block->GetBlockValue("enabled", true))
			/* the plugin is disabled in mpd.conf */
			continue;

		InputPlugin::InitResult result;

		try {
			result = plugin->init != nullptr
				? plugin->init(*block, error)
				: InputPlugin::InitResult::SUCCESS;
		} catch (const PluginUnavailable &e) {
			FormatError(e,
				    "Input plugin '%s' is unavailable",
				    plugin->name);
			continue;
		} catch (const std::runtime_error &e) {
			std::throw_with_nested(FormatRuntimeError("Failed to initialize input plugin '%s'",
								  plugin->name));
		}

		switch (result) {
		case InputPlugin::InitResult::SUCCESS:
			input_plugins_enabled[i] = true;
			break;

		case InputPlugin::InitResult::ERROR:
			error.FormatPrefix("Failed to initialize input plugin '%s': ",
					   plugin->name);
			return false;
		}
	}

	return true;
}

void input_stream_global_finish(void)
{
	input_plugins_for_each_enabled(plugin)
		if (plugin->finish != nullptr)
			plugin->finish();
}
