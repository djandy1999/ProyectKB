using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using System.IO;

public class ServoAngles_v4 : MonoBehaviour
{   
    public GameObject gameObject1;
    public GameObject gameObject2;
    public float storedAngle;
    public float angleThreshold = 5f;
    public Color forwardColor = Color.red;
    public Color angleColor = Color.green;
    public Color directionColor = Color.blue;
    public float lineLength = 1f;

    private BBHWrite BBHWrite;
    void Start(){
        BBHWrite = GameObject.FindObjectOfType<BBHWrite>();

    }
    void Update()
    {
        // Get the transform components of the two game objects.
        Transform object1Transform = gameObject1.transform;
        Transform object2Transform = gameObject2.transform;

        // Calculate the direction vector between the two game objects.
        Vector3 direction = object2Transform.position - object1Transform.position;

        // Normalize the direction vector.
        direction.Normalize();

        // Calculate the signed angle between the forward vector of the first game object and the direction vector.
        float signedAngle = Vector3.SignedAngle(object1Transform.forward, direction, object1Transform.up);

        // Check if the difference between the stored angle and the signed angle is greater than the angle threshold.
        if (Mathf.Abs(storedAngle - signedAngle) >= angleThreshold)
        {
            // Update the stored angle with the calculated signed angle.
            storedAngle = signedAngle;
            if (storedAngle>0){
                BBHWrite.UpdateAngle((int)storedAngle, 15);
            }
            
        }

        // Visualize the forward direction of gameObject1 in the scene.
        Vector3 endPoint = object1Transform.position + object1Transform.forward * lineLength;
        Debug.DrawLine(object1Transform.position, endPoint, forwardColor);

        // Visualize the stored angle trace in the scene.
        Quaternion rotation = Quaternion.AngleAxis(storedAngle, object1Transform.up);
        Vector3 rotatedDirection = rotation * object1Transform.forward;
        endPoint = object1Transform.position + rotatedDirection * lineLength;
        Debug.DrawLine(object1Transform.position, endPoint, angleColor);

        // Visualize the direction vector in the scene.
        endPoint = object1Transform.position + direction * lineLength;
        Debug.DrawLine(object1Transform.position, endPoint, directionColor);
    }

}
