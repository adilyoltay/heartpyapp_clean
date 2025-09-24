export interface RingBufferSnapshot<T> {
  readonly values: T[];
  readonly head: number;
  readonly length: number;
  readonly capacity: number;
  readonly version: number;
}

export class RingBuffer<T> {
  private readonly buffer: T[];
  private head = 0;
  private length = 0;
  private version = 0;

  constructor(private readonly capacity: number) {
    if (capacity <= 0) {
      throw new Error('RingBuffer capacity must be greater than zero');
    }
    this.buffer = new Array(capacity);
  }

  push(item: T): void {
    this.buffer[(this.head + this.length) % this.capacity] = item;
    if (this.length < this.capacity) {
      this.length += 1;
    } else {
      this.head = (this.head + 1) % this.capacity;
    }
    this.version += 1;
  }

  getAll(): T[] {
    const items: T[] = [];
    for (let i = 0; i < this.length; i += 1) {
      items.push(this.buffer[(this.head + i) % this.capacity]);
    }
    return items;
  }

  clear(): void {
    this.head = 0;
    this.length = 0;
    this.version += 1;
  }

  isFull(): boolean {
    return this.length === this.capacity;
  }

  getSize(): number {
    return this.length;
  }

  getLength(): number {
    return this.length;
  }

  getTailIndex(): number {
    return (this.head + this.length - 1) % this.capacity;
  }

  getHeadIndex(): number {
    return this.head;
  }

  snapshot(): RingBufferSnapshot<T> {
    const values = this.getAll();
    return {
      values,
      head: this.head,
      length: this.length,
      capacity: this.capacity,
      version: this.version,
    };
  }
}
