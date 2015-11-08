#Installation in Linux

##Using Debian packet manager
- Add the repo to your sources (needs to be done once):

  ```
  echo "deb https://dl.bintray.com/igagis/deb /" | sudo tee /etc/apt/sources.list.d/igagis.list > /dev/null
  ```

- Update apt packages

  ```
  sudo apt-get update
  ```

- Install **libsvgdom-dev** package

  ```
  sudo apt-get install libsvgren-dev
  ```
