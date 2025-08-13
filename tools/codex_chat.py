import argparse
import json
import os
from typing import Dict

from openai import OpenAI


def load_config(path: str) -> Dict:
    """Load conversation configuration from a JSON file."""
    with open(path, "r", encoding="utf-8") as config_file:
        return json.load(config_file)


def run_conversation(topic: str, options: Dict) -> None:
    """Run a conversation between two GPT models."""
    model_a = options.get("model_a", "gpt-4.1")
    model_b = options.get("model_b", "gpt-4.1-mini")
    temperature = options.get("temperature", 0.5)
    max_tokens = options.get("max_tokens", 150)
    turns = options.get("turns", 4)

    conversation = ""
    last_speaker = "B"  # Let model A speak first.
    client = OpenAI(api_key=os.getenv("OPENAI_API_KEY"))

    for _ in range(turns):
        speaker = "Agent A" if last_speaker == "B" else "Agent B"
        model = model_a if speaker == "Agent A" else model_b
        messages = [
            {
                "role": "system",
                "content": f"The following is a conversation between Agent A and Agent B about {topic}.",
            },
            {"role": "user", "content": f"{conversation}{speaker}:"},
        ]
        completion = client.chat.completions.create(
            model=model,
            messages=messages,
            max_tokens=max_tokens,
            temperature=temperature,
        )
        reply = completion.choices[0].message.content.strip()
        print(f"{speaker}: {reply}")
        conversation += f"{speaker}: {reply}\n"
        last_speaker = "A" if speaker == "Agent A" else "B"


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Run a conversation between two GPT models",
    )
    parser.add_argument(
        "config",
        nargs="?",
        default="codex_chat_config.json",
        help="Path to configuration JSON file",
    )
    args = parser.parse_args()

    api_key = os.getenv("OPENAI_API_KEY")
    if not api_key:
        raise EnvironmentError("OPENAI_API_KEY environment variable is not set")

    config = load_config(args.config)
    topic = config.get("topic", "coding")
    options = config.get("options", {})
    run_conversation(topic, options)


if __name__ == "__main__":
    main()

