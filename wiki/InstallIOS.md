#Installation in iOS and xCode

##Using cocoapods dependency manager
- install **cocoapods**, see http://cocoapods.org for instructions.

- in the Podfile add source repo:

  ```
  source 'https://github.com/igagis/cocoapods-repo.git'
  ```

- in the Podfile add dependency

  ```
  pod 'libsvgren', '>= 0.1.2'
  ```

- istall or update your pods

  ```
  pod install
  ```
  or
  ```
  pod update
  ```
