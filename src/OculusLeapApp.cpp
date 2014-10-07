#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/Rect.h"

#include "Leap.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace Leap;

#define STRINGIFY(s) #s

static const string GLSL_VERT_PASSTHROUGH = STRINGIFY(
    varying vec2 distortionLookup;

    void main() {
      gl_FrontColor = gl_Color;
      distortionLookup = vec2(gl_MultiTexCoord0);
      gl_Position = ftransform();
    }
);

static const string GLSL_FRAG_IMGPROC = STRINGIFY(
    uniform sampler2D   rawData;
    uniform sampler2D   distortion;

    varying vec2 distortionLookup;

    void main(void)
    {
      vec4 indexIntoRawData = texture2D(distortion, distortionLookup);
      
      if(indexIntoRawData.r > 0.0 && indexIntoRawData.r < 1.0
         && indexIntoRawData.g > 0.0 && indexIntoRawData.g < 1.0)
      {
          gl_FragColor = texture2D(rawData, vec2(indexIntoRawData.r,1.0 -indexIntoRawData.g));
      } else {
          gl_FragColor = vec4(0.0, 0, 0, 1.0);
      }
    }
);

class OculusLeapApp : public AppNative {
  public:
	void setup();
	void mouseDown( MouseEvent event );	
	void update();
	void draw();
    
protected:
    Controller      	mLeapController;
    Frame           	mLeapFrame;
    
    gl::Texture         mLeapImage1, mLeapImage2, mLeapDist1, mLeapDist2;
    Surface32f          mDistortionSurface;
    
    gl::GlslProgRef     mWarpShader;
};

void OculusLeapApp::setup()
{
    setFrameRate(60);
    setWindowSize(1280, 720);
    
    Controller::PolicyFlag addImagePolicy = (Controller::PolicyFlag)
    (Controller::POLICY_IMAGES |
//     Controller::POLICY_OPTIMIZE_HMD |
     mLeapController.policyFlags()
    );
    mLeapController.setPolicyFlags(addImagePolicy);
    
    mLeapImage1 = gl::Texture(640, 240);
    mLeapImage2 = gl::Texture(640, 240);
    mLeapDist1 = gl::Texture(64,64);
    mLeapDist2 = gl::Texture(64,64);
    
    try {
        mWarpShader = gl::GlslProg::create(GLSL_VERT_PASSTHROUGH.c_str(), GLSL_FRAG_IMGPROC.c_str());
    } catch (gl::GlslProgCompileExc e) {
        console() << e.what() << endl;
    }
}

void OculusLeapApp::mouseDown( MouseEvent event )
{
}

void OculusLeapApp::update()
{
    if(mLeapController.isConnected())
    {
        mLeapFrame = mLeapController.frame();
        
        if(mLeapFrame.images().count() > 0)
        {
            //get image data
            ImageList images = mLeapFrame.images();
            
            if(!mDistortionSurface)
            {
                console() << "create surface" << endl;
                mDistortionSurface = Surface32f(images[0].distortionWidth(), images[0].distortionHeight(), false);
            }
            
//            for(int i = 0; i < 1; i++)
//            {
                Image image = images[0];
                
                const unsigned char* image_buffer = image.data();
                
                Surface surface(image.width(), image.height(), image.width() * 4, SurfaceChannelOrder::RGBA);
                int cursor = 0;
                Surface::Iter iter = surface.getIter();
                while( iter.line() ) {
                    while( iter.pixel() ) {
                        iter.r() = image_buffer[cursor];
                        iter.g() = iter.b() = iter.r();
                        iter.a() = 255;
                        cursor++;
                    }
                }
                
                const float* distortion_buffer = image.distortion();
                
                //Encode the distorion/calibration map into a texture, r for X, g for Y
                Surface32f distortion(image.distortionWidth()/2, image.distortionHeight(), false);
                cursor = 0;
                Surface32f::Iter dI = distortion.getIter();
                while( dI.line() ) {
                    while( dI.pixel() ) {
                        dI.r() = distortion_buffer[cursor];
                        dI.g() = distortion_buffer[cursor + 1];
                        dI.b() =  dI.a() = 1.0;
                        cursor += 2;
                    }
                }
                
                gl::Texture::Format textureFormat;
                textureFormat.setInternalFormat(GL_RGBA32F_ARB);
                
                gl::Texture distortionTexture(distortion, textureFormat);
                distortionTexture.bind(1);
                
//                if(i)
//                {
                    mLeapDist1.update(distortion);
                    mLeapImage1.update(surface);
//                }
//                else
//                {
//                    mLeapDist2.update(distortion);
//                    mLeapImage2.update(surface);
//                }
//            }
        }
        
    }
}

void OculusLeapApp::draw()
{
	// clear out the window with black
	gl::clear( Color( 0, 0, 0 ) );
    
    
    if(mLeapImage1)
    {
        gl::pushMatrices();
        
        mWarpShader->bind();
        mLeapDist1.bind(1);
        mLeapImage1.bind(3);

        mWarpShader->uniform("rawData", 3);
        mWarpShader->uniform("distortion", 1);
        
        gl::drawSolidRect(getWindowBounds());
        
        mLeapImage1.unbind();
        mLeapDist1.unbind();
        
        mWarpShader->unbind();
        
        gl::popMatrices();
    }
    
    
    //get fingers
    if(mLeapFrame.fingers().count() > 0)
    {
        HandList hands = mLeapFrame.hands();
        
        console() << "hand: \n";
        
        gl::pushMatrices();
        
        //center on screen
        gl::translate(getWindowWidth()/2, getWindowHeight()/2);
        
        for(HandList::const_iterator hl = hands.begin(); hl != hands.end(); hl++)
        {
            Vector pos = (*hl).stabilizedPalmPosition();
            
            console() << "===palm : " << pos << "\n";
            
            gl::color(1.0, 0.0, 0.0);
            gl::drawSphere(Vec3f(-1 * pos.x,pos.z, -1 * pos.y), 20);
        }
        
        FingerList fingers = mLeapFrame.fingers();
        
        
        for(FingerList::const_iterator fl = fingers.begin(); fl != fingers.end(); fl++)
        {
            Vector tip = (*fl).stabilizedTipPosition();
            
            console() << "---tip : " << tip << "\n";
            
            gl::color(1.0, 1.0, 1.0);
            gl::drawSphere(Vec3f(-1 * tip.x,tip.z, -1 * tip.y), 10);
        }
        
        gl::popMatrices();
        
        console() << endl;
    }
}


CINDER_APP_NATIVE( OculusLeapApp, RendererGl )
