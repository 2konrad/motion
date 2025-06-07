<<<<<<< HEAD
Motion
=======
MotionPlus
>>>>>>> plus/upstream__May25
=============

## Description

<<<<<<< HEAD
Motion is a program that monitors the signal from video cameras and detects changes in the images.

## Resources

Please see the [Motion home page](https://motion-project.github.io/) for information regarding building the Motion code from source, documentation of the current and prior releases as well as recent news associated with the application.  Review the [releases page](https://github.com/Motion-Project/motion/releases) for packaged deb files and release notes.  The [issues page](https://github.com/Motion-Project/motion/issues) provides a method to report code bugs while the [discussions page](https://github.com/Motion-Project/motion/discussions) can be used for general questions.

Additionally, there is [Motion User Group List](https://lists.sourceforge.net/lists/listinfo/motion-user) that you can sign up for and submit your question or the [IRC #motion](ircs://irc.libera.chat:6697/motion) on Libera Chat

## License

Motion version 5.0 and later is distributed under the GNU GENERAL PUBLIC LICENSE (GPL) version 3 or later.
=======
MotionPlus is a break from Motion version 4.2.2 to provide an application for enhancements.  MotionPlus removes some of the outdated processes, cleans up the code base and introduces new functionality.

The following are some of the things that are different from Motion.
- Secondary detection method via OpenCV
  - HOG (Histogram of Oriented Gradients)
  - Haar cascade classifiers
  - Deep neural networks(Caffe, TensorFlow, etc.)
- Direct Pi camera support and ability to change camera parameters
- Sound frequency detection
- Additional primary detection parameters
- Sound recording from network camera sources
- ROI pictures for output or secondary detection
- Enhanced web contorl
  - Only needs a single port for all camera video streams and controls
  - Consolidated stream(a single image) showing all cameras
  - List/download movies
  - Add/delete cameras
  - Live view of the Motionplus log output
  - Video streams via MPEGTS format
  - Change/update configuration parameters
  - Permits a user created web page
  - JSON status/configuration pages
  - POST web control processing

## License

MotionPlus is distributed under the GNU GENERAL PUBLIC LICENSE (GPL) version 3 or later.
>>>>>>> plus/upstream__May25


