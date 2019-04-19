# Installation in Linux

## Using Debian packet manager
- Add the repo to your `sources.list` (needs to be done once):
  ```
  deb http://dl.bintray.com/igagis/<distro> <release> main
  ```
  where
  - `<distro>` is `debian` or `ubuntu`
  - `<release>` is `stretch`, `xenial`, `bionic` etc.
  

- Import APT key

  ```
  sudo apt install dirmngr
  sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 379CE192D401AB61
  ```

- Update apt packages

  ```
  sudo apt update
  ```

- Install **libsvgren-dev** package

  ```
  sudo apt install libsvgren-dev
  ```
