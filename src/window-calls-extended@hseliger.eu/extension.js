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

/* exported init */

const { Gio } = imports.gi;

const MR_DBUS_IFACE = `
<node>
    <interface name="org.gnome.Shell.Extensions.WindowsExt">
        <method name="List">
            <arg type="s" direction="out" name="win"/>
        </method>
        <method name="FocusTitle">
            <arg type="s" direction="out" />
        </method>
        <method name="FocusPID">
            <arg type="s" direction="out" />
        </method>
        <method name="FocusClass">
            <arg type="s" direction="out" />
        </method>
    </interface>
</node>`;

class Extension {
    enable() {
        this._dbus = Gio.DBusExportedObject.wrapJSObject(MR_DBUS_IFACE, this);
        this._dbus.export(Gio.DBus.session, '/org/gnome/Shell/Extensions/WindowsExt');
    }

    disable() {
        this._dbus.flush();
        this._dbus.unexport();
        delete this._dbus;
    }
    List() {
        let win = global.get_window_actors()
            .map(a => a.meta_window)
            .map(w => ({ class: w.get_wm_class(), pid: w.get_pid(), id: w.get_id(), maximized: w.get_maximized(), focus: w.has_focus(), title: w.get_title() }));
        return JSON.stringify(win);
    }
    FocusTitle() {
        let win = global.get_window_actors()
            .map(a => a.meta_window)
            .map(w => ({ focus: w.has_focus(), title: w.get_title() }));
        for (let [_ignore , aWindow] of win.entries()) {
            let [focus,theTitle] = Object.entries(aWindow);
            if (focus[1] == true )
                return theTitle[1];
        }
        return "";
    }
    FocusPID() {
        let win = global.get_window_actors()
            .map(a => a.meta_window)
            .map(w => ({ focus: w.has_focus(), pid: w.get_pid() }));
            for (let [_ignore , aWindow] of win.entries()) {
                let [focus,thePID] = Object.entries(aWindow);
                if (focus[1] == true )
                    return ""+thePID[1]; // Turn number into string
            }
            return "";
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
