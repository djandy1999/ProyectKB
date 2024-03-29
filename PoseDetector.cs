using Oculus.Interaction;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
public class PoseDetector : MonoBehaviour
{
    public List<ActiveStateSelector> poses;
    
    private BBH_eRead BBH_eRead;
    public TMPro.TextMeshPro text;

    // Start is called before the first frame update
    void Start()
    {
        foreach (var item in poses)
        {
            item.WhenSelected += () => SetTextToPoseName(item.gameObject.name);
            item.WhenUnselected += () => SetTextToPoseName("0");
        }
        BBH_eRead = GameObject.FindObjectOfType<BBH_eRead>();
    }
    private void SetTextToPoseName(string newText)
    {
        BBH_eRead.updateGesture(newText);
        text.text = newText;
        Debug.Log(newText);
    }
}
