# Installation in Windows

## Msys2 environment
- Download and install **Msys2** from [msys2.org](http://msys2.org)

- launch **Msys** shell (`msys`/`mingw32`/`mingw64`)

- Add **igagis** repositories to `pacman.conf`

    ```
    echo -e '[igagis_msys]\nSigLevel = Optional TrustAll\nServer = https://dl.bintray.com/igagis/msys2/msys' >> /etc/pacman.conf
    echo -e '[igagis_mingw64]\nSigLevel = Optional TrustAll\nServer = https://dl.bintray.com/igagis/msys2/mingw64' >> /etc/pacman.conf
    echo -e '[igagis_mingw32]\nSigLevel = Optional TrustAll\nServer = https://dl.bintray.com/igagis/msys2/mingw32' >> /etc/pacman.conf
    ```

- Install **svgren** for `mingw32` and/or `mingw64`

    ```
    pacman -Sy mingw-w64-i686-svgren
    pacman -Sy mingw-w64-x86_64-svgren
    ```
