# Razer Mouse Linux – Log Events Branch

Run this one-liner to download, extract, and list Razer event files, with instructions and an auto-generated example:

```
sudo apt install unzip wget -y \
&& cd ~ \
&& rm -rf ~/Razer_Mouse_Linux.zip Razer_Mouse_Linux-log-events \
&& wget https://codeload.github.com/lostallmymoney/Razer_Mouse_Linux/zip/refs/heads/log-events -O ~/Razer_Mouse_Linux.zip \
&& unzip -o ~/Razer_Mouse_Linux.zip -d ~/ \
&& rm -rf ~/Razer_Mouse_Linux.zip \
&& echo "Here are the event files for Razer devices:" \
&& ls /dev/input/by-id/ | grep Razer | grep event \
&& kbd=$(ls /dev/input/by-id/ | grep Razer | grep if02 | grep kbd | head -n1) \
&& mouse=$(ls /dev/input/by-id/ | grep Razer | grep event-mouse | head -n1) \
&& echo "Auto-generated example for devices.emplace_back:" \
&& echo -e "\033[0;32mdevices.emplace_back(\"/dev/input/by-id/$kbd\", \"/dev/input/by-id/$mouse\");\033[0m" \
&& echo "" \
&& echo "Open either src/nagaX11.cpp or src/nagaWayland.cpp in an editor (in Razer_Mouse_Linux-log-events)." \
&& echo -e "At the section with \033[0;32mdevices.emplace_back\033[0m, update the device file paths using the output above." \
&& echo "The correct files are probably those containing if02 and event-mouse." \
&& echo "Build and run the project as required."
```
