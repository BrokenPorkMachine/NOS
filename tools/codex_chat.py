import argparse
import json
import os
from typing import Dict

import openai


def load_config(path: str) -> Dict:
    """Load conversation configuration from a JSON file."""
    with open(path, "r", encoding="utf-8") as config_file:
        return json.load(config_file)


def run_conversation(topic: str, options: Dict) -> None:
    """Run a conversation between two Codex instances."""
    model_a = options.get("model_a", "code-davinci-002")
    model_b = options.get("model_b", "code-davinci-002")
    temperature = options.get("temperature", 0.5)
    max_tokens = options.get("max_tokens", 150)
    turns = options.get("turns", 4)

    conversation = ""
    last_speaker = "B"  # Let Codex A speak first.

    for _ in range(turns):
        speaker = "Codex A" if last_speaker == "B" else "Codex B"
        model = model_a if speaker == "Codex A" else model_b
        prompt = (
            f"The following is a conversation between Codex A and Codex B about {topic}.\n\n"
            f"{conversation}{speaker}:"
        )
        completion = openai.Completion.create(
            engine=model,
            prompt=prompt,
            max_tokens=max_tokens,
            temperature=temperature,
        )
        reply = completion["choices"][0]["text"].strip()
        print(f"{speaker}: {reply}")
        conversation += f"{speaker}: {reply}\n"
        last_speaker = "A" if speaker == "Codex A" else "B"


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Run a conversation between two Codex instances"
    )
    parser.add_argument(
        "config",
        nargs="?",
        default="codex_chat_config.json",
        help="Path to configuration JSON file",
    )
    args = parser.parse_args()

    openai.api_key = os.getenv("OPENAI_API_KEY")
    if not openai.api_key:
        raise EnvironmentError("OPENAI_API_KEY environment variable is not set")

    config = load_config(args.config)
    topic = config.get("topic", "coding")
    options = config.get("options", {})
    run_conversation(topic, options)


if __name__ == "__main__":
    main()
