## Software and Control Strategies

The bionic hand's functionality is driven by sophisticated software whose goal is to handle input from various sources, processes data, and control the prosthetic hand's movements through a combination of virtual reality (VR) tracking and electromyography (EMG) signal processing. This section provides an overview of the control strategies and the underlying software architecture.

To accomplish the aforementioned goal this software provides two control modalities: The visual control modality and the myoelectric control modality. Using hand-tracking data from virtual reality (VR) glasses the visual information serves a dual purpose. Firstly, it can be used as a direct control mechanism and secondly, it provides real-time gestural information to train a machine learning algorithm. This trained algorithm enables pattern recognition on muscle signals from a high-density surface electrode array. Thus, the primary modality, namely the myoelectric control, is achieved. A Convolutional Neural Network (CNN) was implemented to predict hand gestures accurately from the myoelectric signals. To refine the predictions different types of pre- and post-processing were implemented. 
 
### Control Strategies

The control system is visualized in a comprehensive control diagram, illustrating the flow of data from input devices to the hand's actuators. The system integrates hand-tracking data from VR glasses and EMG signals from a high-density electrode array to control the hand's movements.

<p align="center">
  <img src="https://github.com/AndyDunkelHell/ProjectBBH/blob/master/Hardware/img/DIAGRAMBACHELOR.png" alt="HandPreview"/>
</p>

#### Input Data Processing
- **Visual Data:** Hand-tracking data from VR glasses is processed using Unity and custom scripts to extract joint angles and gesture information. This data directly controls the servo motors for precise movement replication. This part of the code can be found in the [`Unity`](/Unity/) folder. 

- **Electrode Data:** EMG signals from the forearm are captured through an HD-EMG array, interfaced with an Arduino board equipped with an Intan shield. This data is essential for gesture prediction through machine learning. This part of the code can be found in the [`BBH_Electrodes`](/BBH_Electrodes/) folder.

#### Microcontrollers and Libraries
- **Arduino Uno Rev3 and Portenta H7:** Serve as the core of the control system, with the Portenta H7 managing the main control tasks (on the M7) and running TensorFlow Lite models for gesture prediction (On the M4). This part of the code can be found in the [`PortentaServo`](/PortentaServo/)  and [`PortentaServoM4`](/PortentaServoM4/) folders. 
- **Key Libraries:** Include CommandHandler for serial communication, TensorFlow Lite for running machine learning models, and Adafruit-PWM-Servo-Driver-Library for servo control.

### Training of the Machine Learning Algorithm

The Convolutional Neural Network (CNN) is trained with gesture data from VR tracking and EMG signal data. The training involves:
- Pre-processing the electrode data in Unity.
- Performing a training session to capture gesture data and corresponding EMG patterns.
- Training the CNN with a defined set of gestures, which can be customized per the user's needs.

All the code for this section can be found in the [`Python`](/Python/) folder.

### Myoelectric Control with Trained CNN

After training, the CNN model predicts gestures from EMG data. This prediction is translated into joint angles by the Portenta H7, enabling the bionic hand to replicate the gestures. The system ensures smooth control and real-time responsiveness to user commands. This part is also implemented through [`Unity`](/Unity/) . Both control boards, namely the ArduinoRev3 and the Portenta are connected to Unity using the BBHRead and BBHWrite codes. They then are able to communicate with eachother and all the Data is also saved in the process.

## Implementation and Usage

I will gladly provide further details on implementing the software, setting up the control system, and integrating the hardware and software do not hesitate to contact me! 

## Contributing

I encourage contributions to enhance the software, refine control strategies, and expand the hand's capabilities. Please contact me for more information.

Project BBH 2024
