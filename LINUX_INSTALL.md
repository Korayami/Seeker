# Linux Installation Guide for `seeker`

`seeker` provides packaging for **Arch Linux** (`pacman`, `yay`) and **Debian / Ubuntu / Raspberry Pi** (`apt`).

Both `seeker` and `seek` commands are installed (`seek` is created as a symlink to `seeker`).

---

## ⚡ Single-Line Install Command (Raspberry Pi / Linux / macOS)

Run this single command on your Raspberry Pi (or any Linux machine):

```bash
curl -fsSL https://raw.githubusercontent.com/Korayami/Seeker/main/install.sh | bash
```

*(Or to install without root permissions to `~/.local/bin`: `curl -fsSL https://raw.githubusercontent.com/Korayami/Seeker/main/install.sh | bash -s -- --user`)*

---

## 1. Arch Linux / Manjaro / EndeavourOS (`pacman` / `yay`)

The repository includes a ready-to-use [PKGBUILD](file:///Users/Killian-Alexander.Beck/LocalC/Finder/PKGBUILD).

### Option A: Local Build and Install with `pacman`
Run this directly in the repository directory on your Arch system:
```bash
makepkg -si
```
This will:
1. Compile `seeker` with release optimizations (`-O3`).
2. Package it into a `.pkg.tar.zst` package.
3. Automatically invoke `pacman -U` to install it into `/usr/bin/seeker` and `/usr/bin/seek`.

### Option B: Installing with `yay -S seeker` (AUR)
To enable `yay -S seeker` for yourself and the community:
1. Create a package repository on the Arch User Repository (AUR):
   ```bash
   git clone ssh://aur@aur.archlinux.org/seeker.git
   ```
2. Copy `PKGBUILD` into that folder and generate the `.SRCINFO`:
   ```bash
   makepkg --printsrcinfo > .SRCINFO
   git add PKGBUILD .SRCINFO
   git commit -m "Initial release of seeker v1.0.0"
   git push
   ```
3. Now any user on Arch can install it via:
   ```bash
   yay -S seeker
   ```

---

## 2. Debian / Ubuntu / Linux Mint (`apt`)

The repository includes an automatic Debian package generator in `packaging/debian/build-deb.sh`.

### Option A: Build and Install `.deb` with `apt`
1. Build the `.deb` package:
   ```bash
   make deb
   # or
   ./packaging/debian/build-deb.sh
   ```
   This generates `dist/seeker_1.0.0_<arch>.deb` (e.g. `seeker_1.0.0_amd64.deb`).

2. Install with `apt`:
   ```bash
   sudo apt install ./dist/seeker_1.0.0_amd64.deb
   ```
   *(Using `apt install ./<path>.deb` resolves any local dependencies and registers it cleanly in the apt database).*

3. To uninstall:
   ```bash
   sudo apt remove seeker
   ```

### Option B: Enabling `apt install seeker` via a PPA or Local Repository
To install with standard `apt install seeker` without specifying the `.deb` file path:
- **Via Launchpad PPA (Ubuntu / Debian):**
  Upload the package source to your Launchpad PPA. Users can then install it with:
  ```bash
  sudo add-apt-repository ppa:your-username/seeker
  sudo apt update
  sudo apt install seeker
  ```
- **Via Local APT Repository:**
  Place `seeker_1.0.0_amd64.deb` in a local repository directory, run `dpkg-scanpackages . /dev/null | gzip -9c > Packages.gz`, and add the local path to `/etc/apt/sources.list`.

---

## 3. Direct Source Installation (Any Linux Distribution)

You can also install `seeker` directly using `make` on any Linux distribution:

```bash
sudo make install
```
Or for user-local installation (no root required):
```bash
make install PREFIX=~/.local
```
