import random
import argparse
import os

def generate_operations(n_elements, n_operations, find_ratio, output_filename):
    """
    Generates a file containing Union-Find operations.

    Args:
        n_elements (int): The number of elements in the Union-Find structure (0 to n_elements-1).
        n_operations (int): The total number of operations to generate.
        find_ratio (float): The desired ratio of FIND operations (0.0 to 1.0).
                            A value of 0.9 means ~90% FIND, 10% UNION.
        output_filename (str): The path to the output file.
    """
    if not (0 <= find_ratio <= 1.0):
        raise ValueError("find_ratio must be between 0.0 and 1.0")
    if n_elements <= 0:
        raise ValueError("n_elements must be positive")
    if n_operations <= 0:
        raise ValueError("n_operations must be positive")

    print(f"Generating {n_operations} operations for {n_elements} elements...")
    print(f"Target FIND ratio: {find_ratio:.2f}")
    print(f"Output file: {output_filename}")

    # Ensure the output directory exists
    output_dir = os.path.dirname(output_filename)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)
        print(f"Created directory: {output_dir}")

    try:
        with open(output_filename, 'w') as f:
            # Write header: <num_elements> <num_operations>
            f.write(f"{n_elements} {n_operations}\n")

            generated_ops_count = 0
            find_count = 0
            union_count = 0

            # Generate operations
            for _ in range(n_operations):
                op_type_val = 1 # Default to FIND
                a = random.randint(0, n_elements - 1)
                b = 0 # Default b for FIND

                # Decide operation type based on ratio
                if random.random() > find_ratio:
                    # Generate UNION operation
                    op_type_val = 0
                    # Ensure a != b for UNION, retry if they are the same
                    # (avoids trivial union(x, x))
                    b = random.randint(0, n_elements - 1)
                    while a == b and n_elements > 1: # Prevent infinite loop if n_elements is 1
                         b = random.randint(0, n_elements - 1)
                    union_count += 1
                else:
                    # FIND operation (a already chosen, b is 0)
                    find_count += 1

                # Write operation: <type> <a> <b>
                f.write(f"{op_type_val} {a} {b}\n")
                generated_ops_count += 1

        print("-" * 30)
        print(f"Successfully generated {generated_ops_count} operations.")
        actual_find_ratio = find_count / generated_ops_count if generated_ops_count > 0 else 0
        print(f"Actual FIND operations: {find_count} ({actual_find_ratio:.4f})")
        print(f"Actual UNION operations: {union_count}")
        print("-" * 30)

    except IOError as e:
        print(f"Error writing to file {output_filename}: {e}")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate operation files for Union-Find benchmarking.")

    parser.add_argument("n_elements", type=int, help="Number of elements (e.g., 10000).")
    parser.add_argument("n_operations", type=int, help="Number of operations (e.g., 100000).")
    parser.add_argument("output_file", type=str, help="Path to the output operations file (e.g., datasets/ops_10k_100k.txt).")
    parser.add_argument("--find-ratio", type=float, default=0.5,
                        help="Ratio of FIND operations (0.0 to 1.0, default: 0.5). E.g., 0.9 for find-heavy.")
    parser.add_argument("--seed", type=int, default=None,
                        help="Optional random seed for reproducibility.")

    args = parser.parse_args()

    if args.seed is not None:
        random.seed(args.seed)
        print(f"Using random seed: {args.seed}")

    try:
        generate_operations(args.n_elements, args.n_operations, args.find_ratio, args.output_file)
    except ValueError as e:
        print(f"Error: {e}")

