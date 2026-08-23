VERA Test Console
=================

The VERA Test Console is a Windows desktop application for checking a module
through the supplied USB-to-parallel bridge. The GitHub Actions ``GUI installer``
workflow publishes a self-contained Windows installer artifact; it contains the
application and the required Qt runtime libraries.

Using the console
-----------------

#. Install the ``vera-test-console-<version>-windows-x86_64-setup.exe`` artifact.
#. Connect the bridge and module, then start **VERA Test Console** from the
   Start menu or desktop shortcut.
#. Select the bridge serial port, choose **Connect**, and confirm that a VERA
   version is displayed.
#. Use **VGA signal only** to validate the display path before writing graphics.
#. Run the bitmap, tile, sprite, audio, timing, and SD-card tests as appropriate.

The application transfers VRAM in bounded blocks and reports progress during
large uploads. A failed connection normally indicates a missing serial port,
wrong bridge firmware, or cabling issue. The console does not require a
separate Qt installation on the target computer.
