import os
import requests
import json

def run_agent():
    # Step 1: Fetch data from a network endpoint
    try:
        response = requests.get("https://httpbin.org/json", timeout=10)
        response.raise_for_status()
        raw_data = response.text
    except requests.RequestException as e:
        print(f"Network error: {e}")
        return

    # Step 2: Call Gemini LLM to summarize
    api_key = os.environ.get("AGENTC_API_KEY")
    if not api_key:
        print("Error: AGENTC_API_KEY not set")
        return

    url = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key={api_key}"
    headers = {"Content-Type": "application/json"}
    payload = {
        "contents": [{
            "parts": [{"text": f"Summarize this JSON:\n\n{raw_data}"}]
        }]
    }

    try:
        llm_response = requests.post(url, headers=headers, json=payload, timeout=30)
        llm_response.raise_for_status()
        data = llm_response.json()
        summary = data["candidates"][0]["content"]["parts"][0]["text"]
    except (requests.RequestException, KeyError, IndexError) as e:
        print(f"LLM Error: {e}")
        return

    # Step 3: Write the result to a file
    try:
        with open("summary_python.txt", "w") as f:
            f.write(summary)
    except IOError as e:
        print(f"File error: {e}")
        return

    print("Success: summary_python.txt written.")

if __name__ == "__main__":
    run_agent()
