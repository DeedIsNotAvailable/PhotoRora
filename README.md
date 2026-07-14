[Omsk State Technical University (OmSTU)](https://omgtu.ru)

Application "AI Photo Editor" for local image processing using intelligent tools, for devices running Aurora OS.

Open photos from the gallery and apply AI tools that work completely offline: background removal and replacement, image enhancement, and artistic stylization. The project is focused on private image processing without transferring data to external servers.

## Project Build

- Install Aurora IDE / Aurora SDK and start Aurora OS Build Engine with the target platform you want to build for.
- Install Conan inside the Build Engine before the first CMake configuration of the project.
- Open the repository root in Aurora IDE and create a CMake configuration for the selected Aurora target.
- Use Debug configuration for emulator testing and Release configuration for the final RPM package.

## Package Signing and Installation on Device

- Configure package signing in Aurora IDE or Aurora SDK tools and select the certificate profile for the target emulator or physical device.
- For the simplest workflow, use Aurora IDE Deploy: the IDE builds the RPM package, signs it with the configured profile, and installs it onto the selected emulator or device.
- For manual installation, first sign the Release RPM package using the configured Aurora signing tools.
- Copy the signed package to the target with `scp` if needed, using the SSH port and key configured for the emulator or device in Aurora SDK.
- Install the signed package on the emulator or device using the standard Aurora package installation workflow from the shell or through Aurora IDE deploy tools.
- Keep both the unsigned Release RPM and the signed package until installation is verified successfully on the target.

## Project members

Information about the project authors (developers) is provided in [AUTHORS.md](AUTHORS.md).

## Terms of use

Copyright © 2026 Omsk State Technical University (Chair of Applied Mathematics and Fundamental Computer Science).

The source code of the application is provided under the [BSD-3 Clause](LICENSE.BSD-3-Clause.md) license.

[Project description in Russian](README.ru.md)
