// ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// JeVois Smart Embedded Machine Vision Toolkit - Copyright (C) 2016 by Laurent Itti, the University of Southern
// California (USC), and iLab at USC. See http://iLab.usc.edu and http://jevois.org for information about this project.
//
// This file is part of the JeVois Smart Embedded Machine Vision Toolkit.  This program is free software; you can
// redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software
// Foundation, version 2.  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.  You should have received a copy of the GNU General Public License along with this program;
// if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// Contact information: Laurent Itti - 3641 Watt Way, HNB-07A - Los Angeles, CA 90089-2520 - USA.
// Tel: +1 213 740 3527 - itti@pollux.usc.edu - http://iLab.usc.edu - http://jevois.org
// ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*! \file */

#include <jevois/Core/Module.H>

#include <jevois/Debug/Log.H>
#include <jevois/Debug/Timer.H>
#include <jevois/Image/RawImageOps.H>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/video/background_segm.hpp>

#include <future>
#include <linux/videodev2.h> // for v4l2 pixel types

// icon by by Freepik in arrows at flaticon

//! Simple background subtraction, pretty much straight from the OpenCV tutorials
/*! The background subtraction alorithm learns a statistical model of the appearance of a scene when the camera is not
    moving. Any movin object entering the field of view will then be detected as significantly different from the
    learned pixel model.

    Note that this class has internal state (it learns the statistics of the background over time). FIXME: Unclear how
    it would react to input resolution changes, need to test.

    @author Laurent Itti

    @videomapping YUYV 640 240 15.0 YUYV 320 240 15.0 JeVois DemoBackgroundSubtract
    @videomapping YUYV 320 120 30.0 YUYV 160 120 30.0 JeVois DemoBackgroundSubtract
    @email itti\@usc.edu
    @address University of Southern California, HNB-07A, 3641 Watt Way, Los Angeles, CA 90089-2520, USA
    @copyright Copyright (C) 2016 by Laurent Itti, iLab and the University of Southern California
    @mainurl http://jevois.org
    @supporturl http://jevois.org/doc
    @otherurl http://iLab.usc.edu
    @license GPL v3
    @distribution Unrestricted
    @restrictions None
    \ingroup modules */
class DemoBackgroundSubtract : public jevois::Module
{
  public:
    //! Constructor
    DemoBackgroundSubtract(std::string const & instance) :
        jevois::Module(instance), itsProcessingTimer("Processing"), pMOG2(cv::createBackgroundSubtractorMOG2())
    { }
    
    //! Virtual destructor for safe inheritance
    virtual ~DemoBackgroundSubtract() { }

    //! Processing function
    virtual void process(jevois::InputFrame && inframe, jevois::OutputFrame && outframe) override
    {
      // Wait for next available camera image:
      jevois::RawImage inimg = inframe.get(); unsigned int const w = inimg.width, h = inimg.height;
      inimg.require("input", w, h, V4L2_PIX_FMT_YUYV); // accept any image size but require YUYV pixels

      itsProcessingTimer.start();
      
      // Convert it to BGR:
      cv::Mat imgbgr = jevois::rawimage::convertToCvBGR(inimg);

      // Compute the foreground mask in a thread:
      cv::Mat fgmask;
      auto fg_fut = std::async(std::launch::async, [&]() { pMOG2->apply(imgbgr, fgmask); });
      
      // While computing, wait for an image from our gadget driver into which we will put our results:
      jevois::RawImage outimg = outframe.get();

      // Enforce the correct output image size and format:
      outimg.require("output", w * 2, h, V4L2_PIX_FMT_YUYV);
      
      // Paste the original image to the top-left corner of the display:
      jevois::rawimage::paste(inimg, outimg, 0, 0);
      jevois::rawimage::writeText(outimg, "JeVois Background Subtraction Demo", 3, 3, jevois::yuyv::White);
      
      // Let camera know we are done processing the raw input image:
      inframe.done();

      // Wait for the processing results:
      fg_fut.get();
      
      // Paste the results into the output image:
      jevois::rawimage::pasteGreyToYUYV(fgmask, outimg, w, 0);      
      jevois::rawimage::writeText(outimg, "Foreground Mask", w+3, 3, jevois::yuyv::White);

      // Show processing fps:
      std::string const & fpscpu = itsProcessingTimer.stop();
      jevois::rawimage::writeText(outimg, fpscpu, 3, h - 13, jevois::yuyv::White);

      // Send the output image with our processing results to the host over USB:
      outframe.send();
    }

  protected:
    jevois::Timer itsProcessingTimer;
    cv::Ptr<cv::BackgroundSubtractor> pMOG2;
};

// Allow the module to be loaded as a shared object (.so) file:
JEVOIS_REGISTER_MODULE(DemoBackgroundSubtract);
