#Installation in Linux

##Using Debian packet manager
- Add the repo to your sources (needs to be done once):

  ```
  sudo echo "deb https://repo.fury.io/igagis/ /" > /etc/apt/sources.list.d/igagis.list
  ```

- Update apt packages

  ```
  sudo apt-get update
  ```

- Install **libsvgdom-dev** package

  ```
  sudo apt-get install libsvgdom-dev
  ```
