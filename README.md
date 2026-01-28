# Multithreading-Learning-on-Linux

15 Chapters of learning multithreading on Linux using C.

My Enviornment:  
OS: MacOS  
IDE: VS Code  
Language: C  

## Setup Enviornment
### 1. Download `Docker` [here](https://www.docker.com/).
### 2. Install `Dev Containers` (by Microsoft) in "Extensions" in VS Code.
#### 2.1 Create a new folder `c_thread_pool` in VS Code an open it.
#### 2.2 Initialize the Container
- Press `Cmd+Shift+P` to open the Command Palette.
- Type and select: **Dev Containers: Add Dev Container Configuration Files...**
- Select **Show All Definitions...**
- Choose a generic Linux version, such as **Ubuntu** (select the latest version, e.g., jammy or noble).
#### 2.3 Build and Open
- VS Code will create a `.devcontainer` folder.
- A notification will pop up asking to "Reopen in Container". Click **Reopen in Container**.
- Wait.
### 3. Verify if Linux is installed permanantly.
Type `uname`, should show **Linux (Ubuntu)**, not macOS.
