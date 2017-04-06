# Installation in Linux

## Using Debian packet manager
- Add the repo to your sources (needs to be done once):

  **Debian**
  ```
  echo "deb https://dl.bintray.com/igagis/deb unstable main" | sudo tee /etc/apt/sources.list.d/igagis.list > /dev/null
  ```
  **Ubuntu**
  ```
  echo "deb https://dl.bintray.com/igagis/ubu unstable main" | sudo tee /etc/apt/sources.list.d/igagis.list > /dev/null
  ```

- Update apt packages

  ```
  sudo apt-get update
  ```

- Install **libsvgren-dev** package

  ```
  sudo apt-get install libsvgren-dev
  ```
