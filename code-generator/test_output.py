# StreamPunk Python Data Types
# This is a test implementation
from enum import Enum
from typing import Any, Optional

class E_StreamPunkType(Enum):
    # TODO: Add enum values here
    pass

class Base:
    def to(self, o: Any) -> None:
        # TODO: implement
        pass

    def from_dict(self, data: dict) -> Any:
        # TODO: implement
        pass

# TODO: Add custom type classes here

def read_obj(i: Any) -> Any:
    # TODO: implement
    pass

def write_obj(o: Any, value: Any) -> None:
    # TODO: implement
    pass
