/* extension.js
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Created: 2022-04-05T09:02:24+02:00 by Hendrik G. Seliger (github@hseliger.eu)
// Last changes: 2022-04-05T13:09:46+02:00 by Hendrik G. Seliger (github@hseliger.eu)

// Based on the initial version by ickyicky (https://github.com/ickyicky),
// extended by a few methods to provide the focused window's title, window class, and pid.


// Notice : github.com/lostallmymoney, I, stripped the extention of what wasn't needed for github.com/lostallmymoney/Razer_Mouse_Linux

/* exported init */

import Gio from 'gi://Gio';

const MR_DBUS_IFACE = `
<node>
    <interface name="org.gnome.Shell.Extensions.WindowsExt">
        <method name="FocusClass">
            <arg type="s" direction="out" />
        </method>
    </interface>
</node>`;

export default class WCExtension {
    enable() {
        this._dbus = Gio.DBusExportedObject.wrapJSObject(MR_DBUS_IFACE, this);
        this._dbus.export(Gio.DBus.session, '/org/gnome/Shell/Extensions/WindowsExt');
    }

    disable() {
        this._dbus.flush();
        this._dbus.unexport();
        delete this._dbus;
    }
    FocusClass() {
        let win = global.get_window_actors()
            .map(a => a.meta_window)
            .map(w => ({ focus: w.has_focus(), class: w.get_wm_class() }));
            for (let [_ignore , aWindow] of win.entries()) {
                let [focus,theClass] = Object.entries(aWindow);
                if (focus[1] == true )
                    return theClass[1];
            }
            return "";
        }
}

function init() {
    return new Extension();
}
